/**
 * @file llpreviewscript.cpp
 * @brief LLPreviewScript and LLLiveLSLEditor classes implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * Preprocessor code (c) 2019 Henri Beauchamp.
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

#include <deque>

#include "llpreviewscript.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lleventtimer.h"
#include "llexperiencecache.h"
#include "hbexternaleditor.h"
#include "llfilesystem.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloaterexperienceprofile.h"
#include "llfloatersearchreplace.h"
#include "llgridmanager.h"
#include "llinventorymodel.h"
#include "llmediactrl.h"
#include "hbpreprocessor.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llweb.h"
#include "roles_constants.h"

const std::string HELLO_LSL =
	"default {\n"
	"    state_entry() {\n"
    "        llOwnerSay(llGetScriptName() + \": Hello, Avatar !\");\n"
    "    }\n"
	"\n"
	"    touch_start(integer total_number) {\n"
	"        llWhisper(0, llGetScriptName() + \": Touched.\");\n"
	"    }\n"
	"}\n";

// *TODO: translate ?
const std::string DEFAULT_SCRIPT_NAME = "New script";

const std::string ESCAPED_SOURCES_MARKER =
	"//********** Escaped, original, non-preprocessed sources **********//\n";
const std::string ESCAPE_STRING = "//* ";
const std::string ESCAPED_INCLUDE_MARKER =
	"//********** Non-preprocessed include sources **********//\n";
const std::string ESCAPED_INCLUDE_FOOTER =
	"//********* End of non-preprocessed include sources *********//\n";
const std::string DUMMY_STATE =
	"\ndefault { state_entry() { llOwnerSay(\"This is an #include script.\"); } }\n";

const std::string ALIEN_ESCAPED_START_MARKER = "//start_unprocessed_text\n/*";
const std::string ALIEN_ESCAPED_END_MARKER = "*/\n//end_unprocessed_text";

constexpr S32 SCRIPT_BORDER = 4;
constexpr S32 SCRIPT_PAD = 5;
constexpr S32 SCRIPT_BUTTON_WIDTH = 128;
constexpr S32 SCRIPT_BUTTON_HEIGHT = 24;	// HACK: Use gBtnHeight where possible.
constexpr S32 LINE_COLUMN_HEIGHT = 14;
constexpr S32 SCRIPT_EDITOR_MIN_HEIGHT = 2 * SCROLLBAR_SIZE +
										 2 * LLPANEL_BORDER_WIDTH + 128;
constexpr S32 SCRIPT_MIN_WIDTH = 2 * SCRIPT_BORDER + 2 * SCRIPT_BUTTON_WIDTH +
								 SCRIPT_PAD + RESIZE_HANDLE_WIDTH + SCRIPT_PAD;
constexpr S32 SCRIPT_MIN_HEIGHT = 2 * SCRIPT_BORDER +
								  3 * (SCRIPT_BUTTON_HEIGHT + SCRIPT_PAD) +
							  LINE_COLUMN_HEIGHT + SCRIPT_EDITOR_MIN_HEIGHT;
constexpr S32 MAX_HISTORY_COUNT = 10;
constexpr F32 LIVE_HELP_REFRESH_TIME = 1.f;
constexpr F32 AUTO_SAVE_INTERVAL = 60.f;

static bool have_script_upload_cap(LLUUID object_id)
{
	LLViewerRegion* region = NULL;
	if (object_id.isNull())
	{
		region = gAgent.getRegion();
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(object_id);
		if (object)
		{
			region = object->getRegion();
		}
	}
	return region && !region->getCapability("UpdateScriptTask").empty();
}

// ----------------------------------------------------------------------------
// LLScriptEditor class
// Inner implementation class for use by LLPreviewScript and LLLiveLSLEditor.
// ----------------------------------------------------------------------------

class LLScriptEditor : public LLPanel, public LLEventTimer
{
	friend class LLPreviewScript;
	friend class LLLiveLSLEditor;

protected:
	LOG_CLASS(LLScriptEditor);

public:
	LLScriptEditor(const LLUUID& item_id, void (*load_cb)(void*),
				   void (*save_cb)(void*, bool), void (*search_cb)(void*),
				   void* userdata);
	~LLScriptEditor() override;

	void draw() override;

	virtual bool canClose();

	bool handleKeyHere(KEY key, MASK mask) override;

	void autoSave();

	// LLEventTimer API
	bool tick() override;

	void setScriptName(std::string name);

	std::string getItemPath();

	void setScriptText(std::string text, bool is_valid, bool set_saved = true);

	void setEditedTextFromSaved();

	void loadFile(const std::string& filename);

	bool handleSaveChangesDialog(const LLSD& notification,
								 const LLSD& response);
	bool handleReloadFromServerDialog(const LLSD& notification,
									  const LLSD& response);

	void addComment(const std::string& comment, bool is_error = false);

	LL_INLINE LLCheckBoxCtrl* getMonoCheckBox()			{ return mMonoCheckbox; }
	LL_INLINE bool monoChecked() const					{ return mMonoCheckbox->get(); }

	void selectFirstError();

	LL_INLINE void enableSave(bool b)					{ mEnableSave = b; }
	void enableEdit(bool enable);

	LL_INLINE LLUUID getAssociatedExperience() const	{ return mAssociatedExperience; }

	LL_INLINE void setAssociatedExperience(const LLUUID& exp_id)
	{
		mAssociatedExperience = exp_id;
	}

 	LL_INLINE const char* getTitleName() const
	{
		return "Script";
	}

	LL_INLINE bool hasChanged()
	{
		return mHasScriptData && (mEnableSave || !mEditor->isPristine());
	}

	static void loadFunctions(const std::string& filename);

private:
	enum
	{
		PREPROCESS_START = 0,
		PREPROCESS_WAITING,
		PREPROCESS_RESUME,
		PREPROCESS_DONE
	};

	void doSave(bool close_after_save, bool check_preprocessing = true);
	void preprocess();
	bool loadAsset(LLViewerInventoryItem* item);

	void setHelpPage(const std::string& help_string);
	void updateDynamicHelp(bool immediate = false);
	void addHelpItemToHistory(const std::string& help_string);

	static void onIdle(void* userdata);
	static LLViewerInventoryItem* getScriptItem(const std::string& name);
	static void onLoadComplete(const LLUUID& asset_id, LLAssetType::EType type,
							   void* userdata, S32 status, LLExtStat);
	static S32 loadInclude(std::string& include_name, const std::string& path,
						   std::string& buffer, void* userdata);
	static void preprocessorMessage(const std::string& message,
									bool is_warning, void* userdata);
	static std::string escapeSources(const std::string& sources);
	static std::string unescapeSources(const std::string& sources);
	static std::string removeEscapedSources(const std::string& sources);
	static std::string setIncludeSources(const std::string& sources);
	static std::string getIncludeSources(const std::string& sources);
	static std::string convertSources(const std::string& sources);

	static void loadFromFileCallback(HBFileSelector::ELoadFilter type,
									 std::string& filename, void* userdata);
	static void saveToFileCallback(HBFileSelector::ESaveFilter type,
								   std::string& filename, void* userdata);

	static void onEditedFileChanged(const std::string& filename,
									void* userdata);

	static bool onHelpWebDialog(const LLSD& notification,
								const LLSD& response);
	static void onBtnHelp(void* userdata);
	static void onBtnDynamicHelp(void* userdata);
	static void onHelpFollowCursor(void*);
	static void onCheckLock(LLUICtrl* ctrl, void* userdata);
	static void onHelpComboCommit(LLUICtrl* ctrl, void* userdata);
	static void onClickBack(void* userdata);
	static void onClickForward(void* userdata);
	static void onBtnInsertFunction(LLUICtrl*, void* userdata);
	static void onMonoCheckboxClicked(LLUICtrl*, void* userdata);
	static void onBtnLoadFromFile(void* userdata);
	static void onBtnSaveToFile(void* userdata);
	static void onEditExternal(void* userdata);
	static void onEditRaw(void* userdata);
	static void onBtnSave(void* userdata);
	static void onFlyoutBtnSave(LLUICtrl* ctrl, void* userdata);
	static void onBtnUndoChanges(void* userdata);
	static void onSearchMenu(void* userdata);

	static void onUndoMenu(void* userdata);
	static void onRedoMenu(void* userdata);
	static void onCutMenu(void* userdata);
	static void onCopyMenu(void* userdata);
	static void onPasteMenu(void* userdata);
	static void onSelectAllMenu(void* userdata);
	static void onDeselectMenu(void* userdata);

	static void onErrorList(LLUICtrl*, void* user_data);

	static bool enableLoadFile(void* userdata);
	static bool enableSaveFile(void* userdata);
	static bool enableRaw(void* userdata);
	static bool enableCallback(void* userdata);

	static bool enableUndoMenu(void* userdata);
	static bool enableRedoMenu(void* userdata);
	static bool enableCutMenu(void* userdata);
	static bool enableCopyMenu(void* userdata);
	static bool enablePasteMenu(void* userdata);
	static bool enableSelectAllMenu(void* userdata);
	static bool enableDeselectMenu(void* userdata);

	static bool enableHelp(void* userdata);

public:
	static LLFontGL*		sCustomFont;

private:
	void					(*mLoadCallback)(void* userdata);
	void					(*mSaveCallback)(void* userdata,
											 bool close_after_save);
	void					(*mSearchReplaceCallback) (void* userdata);
	void*					mUserdata;

	HBPreprocessor*			mPreprocessor;

	LLTabContainer*			mTabContainer;
	LLButton*				mSaveButton;
	LLFlyoutButton*			mSaveFlyoutButton;
	LLTextBox*				mLineColText;
	LLComboBox*				mFunctions;
	LLTextEditor*			mEditor;
	LLTextEditor*			mSavedSources;
	LLCheckBoxCtrl*			mMonoCheckbox;
	LLScrollListCtrl*		mErrorList;

	LLKeywordToken* 		mLastHelpToken;
	S32						mLiveHelpHistorySize;
	LLHandle<LLFloater>		mLiveHelpHandle;

	HBExternalEditor*		mExternalEditor;

	LLUUID					mItemUUID;
	LLUUID					mAssociatedExperience;

	S32						mPreprocessState;

	F32						mLastPosUpdate;
	F32						mLastHelpUpdate;

	std::string				mScriptName;
	std::string				mAutosaveFilename;

	bool					mForceClose;
	bool					mCloseAfterSave;
	bool					mNeedSaving;
	bool					mEnableSave;
	bool					mIsSaving;
	bool					mHasScriptData;
	bool					mSaveDialogShown;

	typedef fast_hset<LLScriptEditor*> instances_list_t;
	static instances_list_t	sInstances;

	struct LSLFunctionProps
	{
		LSLFunctionProps(const std::string& name, const std::string& tooltip,
						 F32 sleep_time, bool god_only)
		:	mName(name),
			mTooltip(tooltip),
			mSleepTime(sleep_time),
			mGodOnly(god_only)
		{
		}

		F32			mSleepTime;
		std::string	mName;
		std::string	mTooltip;
		bool		mGodOnly;
	};
	typedef std::vector<LSLFunctionProps> functions_table_t;
	static functions_table_t sParsedFunctions;
};

// static members
LLScriptEditor::instances_list_t LLScriptEditor::sInstances;
LLScriptEditor::functions_table_t LLScriptEditor::sParsedFunctions;
LLFontGL* LLScriptEditor::sCustomFont = NULL;

struct LLSECKeywordCompare
{
	LL_INLINE bool operator()(const std::string& lhs, const std::string& rhs)
	{
		return LLStringUtil::compareDictInsensitive(lhs, rhs) < 0;
	}
};

LLScriptEditor::LLScriptEditor(const LLUUID& item_id, void (*load_cb)(void*),
							   void (*save_cb)(void*, bool),
							   void (*search_cb)(void*), void* userdata)
:	LLPanel("panel_script_editor"),
	mItemUUID(item_id),
	mScriptName("untitled"),
	mLoadCallback(load_cb),
	mSaveCallback(save_cb),
	mSearchReplaceCallback(search_cb),
	mUserdata(userdata),
	mPreprocessor(NULL),
	mPreprocessState(PREPROCESS_WAITING),
	mLastHelpToken(NULL),
	mExternalEditor(NULL),
	mLiveHelpHistorySize(0),
	mCloseAfterSave(false),
	mForceClose(false),
	mNeedSaving(false),
	mEnableSave(false),
	mIsSaving(false),
	mHasScriptData(false),
	mSaveDialogShown(false),
	mLastPosUpdate(0.f),
	mLastHelpUpdate(0.f),
	LLEventTimer(AUTO_SAVE_INTERVAL)
{
	sInstances.insert(this);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_script_edit.xml");

	mTabContainer = getChild<LLTabContainer>("sources");

	mErrorList = getChild<LLScrollListCtrl>("lsl errors");
	mErrorList->setCommitCallback(onErrorList);
	mErrorList->setCallbackUserData(this);

	mFunctions = getChild<LLComboBox>("insert_combo");
	mFunctions->setCommitCallback(onBtnInsertFunction);
	mFunctions->setCallbackUserData(this);

	mEditor = getChild<LLViewerTextEditor>("unprocessed_script");
	mEditor->setHandleEditKeysDirectly(true);
	if (sCustomFont)
	{
		mEditor->setFont(sCustomFont);
	}

	mSavedSources = getChild<LLViewerTextEditor>("preprocessed_script");
	mSavedSources->setHandleEditKeysDirectly(true);
	if (sCustomFont)
	{
		mSavedSources->setFont(sCustomFont);
	}

	mMonoCheckbox =	getChild<LLCheckBoxCtrl>("mono");
	mMonoCheckbox->setCommitCallback(onMonoCheckboxClicked);
	mMonoCheckbox->setCallbackUserData(this);
	mMonoCheckbox->setEnabled(false);
	mMonoCheckbox->setVisible(gIsInSecondLife);

	std::vector<std::string> funcs, tooltips;
	for (functions_table_t::const_iterator it = sParsedFunctions.begin(),
										   end = sParsedFunctions.end();
		 it != end; ++it)
	{
		// Make sure this is not a god only function, or the agent is a god.
		if (!it->mGodOnly || gAgent.isGodlike())
		{
			std::string name = it->mName;
			funcs.emplace_back(name);

			std::string desc = it->mTooltip;
			F32 sleep_time = it->mSleepTime;
			if (sleep_time)
			{
				desc += "\n";

				LLStringUtil::format_map_t args;
				args["[SLEEP_TIME]"] = llformat("%.1f", sleep_time);
				desc += LLTrans::getString("LSLTipSleepTime", args);
			}

			// A \n linefeed is not part of xml. Let's add one to keep all
			// the tips one-per-line in strings.xml
			LLStringUtil::replaceString(desc, "\\n", "\n");

			tooltips.emplace_back(desc);
		}
	}

	LLColor3 color = LLColor3(gColors.getColor("LslFunctionTextFgColor"));
	std::string keysfile = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														  "keywords.ini");
	mEditor->loadKeywords(keysfile, funcs, tooltips, color);
	mSavedSources->loadKeywords(keysfile, funcs, tooltips, color);

	std::vector<std::string> primary_keywords, secondary_keywords;
	for (LLKeywords::keyword_iterator_t it = mEditor->keywordsBegin(),
										end = mEditor->keywordsEnd();
		 it != end; ++it)
	{
		LLKeywordToken* token = it->second;
		if (!token) continue; // Paranoia

		// *HACK: sort tokens based on their highligthing colors... Better not
		// using the same highligthing color for all...
		if (token->getColor() == color)
		{
			primary_keywords.emplace_back(wstring_to_utf8str(token->getToken()));
		}
		else
		{
			secondary_keywords.emplace_back(wstring_to_utf8str(token->getToken()));
		}
	}

	// Case-insensitive dictionary sort for primary keywords. We do not sort
	// the secondary keywords. They are intelligently grouped in keywords.ini.
	std::stable_sort(primary_keywords.begin(), primary_keywords.end(),
					 LLSECKeywordCompare());

	for (std::vector<std::string>::const_iterator
			it = primary_keywords.begin(), end = primary_keywords.end();
		 it != end; ++it)
	{
		mFunctions->add(*it);
	}

	for (std::vector<std::string>::const_iterator
			it = secondary_keywords.begin(), end = secondary_keywords.end();
		 it != end; ++it)
	{
		mFunctions->add(*it);
	}

	mSaveButton = getChild<LLButton>("save_btn");
	mSaveButton->setClickedCallback(onBtnSave, this);

	mSaveFlyoutButton = getChild<LLFlyoutButton>("save_flyout_btn");
	mSaveFlyoutButton->setCommitCallback(onFlyoutBtnSave);
	mSaveFlyoutButton->setCallbackUserData(this);

	bool is_inventory = gInventory.getItem(mItemUUID) != NULL;
	mSaveButton->setVisible(!is_inventory);
	mSaveFlyoutButton->setVisible(is_inventory);

	mLineColText = getChild<LLTextBox>("line_col");

	LLMenuItemCallGL* item = getChild<LLMenuItemCallGL>("load");
	item->setMenuCallback(onBtnLoadFromFile, this);
	item->setEnabledCallback(enableLoadFile);

	item = getChild<LLMenuItemCallGL>("save");
	item->setMenuCallback(onBtnSaveToFile, this);
	item->setEnabledCallback(enableSaveFile);

	item = getChild<LLMenuItemCallGL>("external");
	item->setMenuCallback(onEditExternal, this);
	item->setEnabledCallback(enableLoadFile);

	item = getChild<LLMenuItemCallGL>("raw");
	item->setMenuCallback(onEditRaw, this);
	item->setEnabledCallback(enableRaw);

	item = getChild<LLMenuItemCallGL>("revert");
	item->setMenuCallback(onBtnUndoChanges, this);
	item->setEnabledCallback(enableCallback);

	item = getChild<LLMenuItemCallGL>("undo");
	item->setMenuCallback(onUndoMenu, this);
	item->setEnabledCallback(enableUndoMenu);

	item = getChild<LLMenuItemCallGL>("redo");
	item->setMenuCallback(onRedoMenu, this);
	item->setEnabledCallback(enableRedoMenu);

	item = getChild<LLMenuItemCallGL>("cut");
	item->setMenuCallback(onCutMenu, this);
	item->setEnabledCallback(enableCutMenu);

	item = getChild<LLMenuItemCallGL>("copy");
	item->setMenuCallback(onCopyMenu, this);
	item->setEnabledCallback(enableCopyMenu);

	item = getChild<LLMenuItemCallGL>("paste");
	item->setMenuCallback(onPasteMenu, this);
	item->setEnabledCallback(enablePasteMenu);

	item = getChild<LLMenuItemCallGL>("select_all");
	item->setMenuCallback(onSelectAllMenu, this);
	item->setEnabledCallback(enableSelectAllMenu);

	item = getChild<LLMenuItemCallGL>("deselect");
	item->setMenuCallback(onDeselectMenu, this);
	item->setEnabledCallback(enableDeselectMenu);

	item = getChild<LLMenuItemCallGL>("search");
	item->setMenuCallback(onSearchMenu, this);
	item->setEnabledCallback(NULL);

	item = getChild<LLMenuItemCallGL>("wiki");
	item->setMenuCallback(onBtnHelp, this);
	item->setEnabledCallback(NULL);

	item = getChild<LLMenuItemCallGL>("help");
	item->setMenuCallback(onBtnDynamicHelp, this);
	item->setEnabledCallback(enableHelp);

	LLMenuItemCheckGL* check = getChild<LLMenuItemCheckGL>("dynamic");
	check->setMenuCallback(onHelpFollowCursor, this);
	check->setEnabledCallback(enableHelp);

	// Tell LLEditMenuHandler about our editor type: this will trigger a Lua
	// callback if one is configured for context menus. HB
	mEditor->setCustomMenuType("script");
}

//virtual
LLScriptEditor::~LLScriptEditor()
{
	sInstances.erase(this);
	gIdleCallbacks.deleteFunction(onIdle, this);
	if (mPreprocessor)
	{
		delete mPreprocessor;
	}
	if (mExternalEditor)
	{
		delete mExternalEditor;
	}
}

//virtual
void LLScriptEditor::draw()
{
	bool changed = hasChanged();
	mSaveButton->setEnabled(changed);
	mSaveFlyoutButton->setEnabled(changed);

	// Do not do this every frame !
	if (gFrameTimeSeconds > mLastPosUpdate + 0.25f)
	{
		if (mEditor->hasFocus())
		{
			S32 row = 0;
			S32 col = 0;
			// false = do not include wordwrap
			mEditor->getCurrentLineAndColumn(&row, &col, false);
			mLineColText->setText(llformat("Line %d, Column %d", row, col));
		}
		else
		{
			mLineColText->setText(LLStringUtil::null);
		}
		mLastPosUpdate = gFrameTimeSeconds;
	}

	// Do not do this every frame !
	if (gFrameTimeSeconds > mLastHelpUpdate + LIVE_HELP_REFRESH_TIME)
	{
		updateDynamicHelp();
	}

	LLPanel::draw();
}

//virtual
bool LLScriptEditor::canClose()
{
	if (mForceClose || !hasChanged())
	{
		return true;
	}

	if (!mSaveDialogShown)
	{
		mSaveDialogShown = true;
		// Bring up view-modal dialog: Save changes ? Yes, No, Cancel
		gNotifications.add("SaveChanges", LLSD(), LLSD(),
						   boost::bind(&LLScriptEditor::handleSaveChangesDialog,
									   this, _1, _2));
	}

	return false;
}

//virtual
bool LLScriptEditor::handleKeyHere(KEY key, MASK mask)
{
	if ((mask & MASK_MODIFIERS) == MASK_CONTROL)
	{
		if (key == 'S')
		{
			// false = do not close after saving
			doSave(false);
			return true;
		}

		if (key == 'F')
		{
			if (mSearchReplaceCallback)
			{
				mSearchReplaceCallback(mUserdata);
			}
			return true;
		}
	}
	return false;
}

void LLScriptEditor::autoSave()
{
	if (mAutosaveFilename.empty())
	{
		std::string filename = gDirUtilp->getTempFilename(false) + ".lsl";
		mAutosaveFilename = filename;
	}

	if (mExternalEditor)
	{
		// Do not cause a file changed event for something we trigger ourselves
		// (the external editor will cause a file access read event, which is
		// considered a changed event, and would cause HBExternalEditor to call
		// our own changed file event, which we do not want to happen here).
		mExternalEditor->ignoreNextUpdate();
	}

	LLFILE* fp = LLFile::open(mAutosaveFilename, "wb");
	if (!fp)
	{
		llwarns << "Unable to write to " << mAutosaveFilename << llendl;
		addComment(getString("cannot_write"), true);
		return;
	}

	// Note: we save the edited (not (yet) preprocessed) text, not the saved
	// (and preprocessed) one.
	std::string text = mEditor->getText();
	if (text.empty())
	{
		// Special case for a completely empty script; stuff in one new line so
		// that it can store properly. See SL-46889
		text = "\n";
	}
	fputs(text.c_str(), fp);
	LLFile::close(fp);

	llinfos << "Auto-saved: " << mAutosaveFilename << llendl;
}

//virtual
bool LLScriptEditor::tick()
{
	// Do not auto-save when nothing changed or the text is being edited in an
	// external text editor.
	if (!mEditor->isPristine() &&
		!(mExternalEditor && mExternalEditor->running()))
	{
		autoSave();
	}
	return false;
}

void LLScriptEditor::addComment(const std::string& comment, bool is_error)
{
	if (is_error)
	{
		LLSD row;
		LLSD& column = row["columns"][0];
		column["value"] = comment;
		column["font"] = "SMALL";
		column["color"] = LLColor4::red2.getValue();
		mErrorList->addElement(row);
	}
	else
	{
		mErrorList->addCommentText(comment);
	}
	mErrorList->scrollToShowLast();
}

void LLScriptEditor::enableEdit(bool enable)
{
	mIsSaving = !enable;
	mEditor->setEnabled(enable);
}

std::string LLScriptEditor::getItemPath()
{
	std::string path;
	LLInventoryItem* item = gInventory.getItem(mItemUUID);
	if (!item)
	{
		// Not in inventory
		return path;
	}

	if (!gInventory.isObjectDescendentOf(mItemUUID,
										 gInventory.getRootFolderID()))
	{
		// Not in user inventory (i.e. it is a library item)
		return path;
	}

	// Find the full inventory path for the item
	path = "|";	// Start at root inventory
	const LLUUID& root_id = gInventory.getRootFolderID();
	LLUUID cat_id = item->getParentUUID();
	while (cat_id != root_id)
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		if (!cat)
		{
			// Something is very wrong... Give up !
			path.clear();
			break;
		}
		path = "|" + cat->getName() + path;
		cat_id = cat->getParentUUID();
	}

	return path;
}

void LLScriptEditor::preprocess()
{
	if (mPreprocessState == PREPROCESS_START)
	{
		enableEdit(false);
		addComment(getString("preprocessing"));

		if (!mPreprocessor)
		{
			std::string item_path = getItemPath();
			mPreprocessor = new HBPreprocessor(item_path + mScriptName,
											   loadInclude, this);
			mPreprocessor->setMessageCallback(preprocessorMessage);
		}

		if (mPreprocessor->preprocess(mEditor->getText()) ==
				HBPreprocessor::PAUSED)
		{
			// We need to wait till an #include script asset gets loaded...
			mPreprocessState = PREPROCESS_WAITING;
			gIdleCallbacks.addFunction(onIdle, this);
			return;
		}

		// Note: we are also done in case of error
		mPreprocessState = PREPROCESS_DONE;
	}

	// mPreprocessState is set to PREPROCESS_RESUME when an #included asset has
	// successfully loaded. Should it fail to load, the state would be set to
	// PREPROCESS_DONE.
	if (mPreprocessState == PREPROCESS_RESUME)
	{
		if (mPreprocessor && mPreprocessor->resume() == HBPreprocessor::PAUSED)
		{
			mPreprocessState = PREPROCESS_WAITING;
			return;
		}
		// Note: we are also done in case of error
		mPreprocessState = PREPROCESS_DONE;
	}
		
	if (mPreprocessState == PREPROCESS_DONE)
	{
		gIdleCallbacks.deleteFunction(onIdle, this);
		mSavedSources->setText(mPreprocessor->getResult() +
							   escapeSources(mEditor->getText()));
		addComment(getString("done"));
		enableEdit(true);
		mPreprocessState = PREPROCESS_WAITING;
		if (mNeedSaving)
		{
			doSave(mCloseAfterSave, false);
		}
	}
}

void LLScriptEditor::setScriptText(std::string text, bool is_valid,
								   bool set_saved)
{
	mHasScriptData = is_valid;

	mErrorList->deleteAllItems();

	if (set_saved)
	{
		// Set sources "as is" in the "Saved script" tab editor
		mSavedSources->setText(text);
	}

	if (text.find(ALIEN_ESCAPED_START_MARKER) != std::string::npos)
	{
		text = convertSources(text);
	}

	if (text.find(ESCAPED_INCLUDE_MARKER) != std::string::npos)
	{
		text = getIncludeSources(text);
	}
	else if (text.find(ESCAPED_SOURCES_MARKER) != std::string::npos)
	{
		text = unescapeSources(text);
	}

	// Set cleaned up, non-processed sources in the "Edited script" tab editor
	mEditor->setText(text);
}

void LLScriptEditor::setEditedTextFromSaved()
{
	if (mHasScriptData)
	{
		mEditor->setText(mSavedSources->getText());
	}
}

void LLScriptEditor::setScriptName(std::string name)
{
	if (name.find("Script: ") == 0)
	{
		name = name.substr(8);
	}
	if (name.empty())
	{
		name = "untitled";
	}
	mScriptName = name;
	if (mPreprocessor)
	{
		mPreprocessor->setFilename(name);
	}
}

void LLScriptEditor::doSave(bool close_after_save, bool check_preprocessing)
{
	mCloseAfterSave = close_after_save;
	mIsSaving = true;
	std::string text = mEditor->getText();
	if (!mHasScriptData || text.empty())
	{
		llwarns << "Nothing to save" << llendl;
		return;
	}

	if (!mSaveCallback)
	{
		llwarns << "No save callback !" << llendl;
		return;
	}

	if (check_preprocessing)
	{
		mErrorList->deleteAllItems();

		if (HBPreprocessor::needsPreprocessing(text))
		{
			mNeedSaving = true;
			mPreprocessState = PREPROCESS_START;
			preprocess();
			return;
		}

		mSavedSources->setText(text);
		if (mPreprocessor)
		{
			mPreprocessor->clear();
		}
	}
	else
	{
		mNeedSaving = false;
	}
	if (!close_after_save && mExternalEditor && mExternalEditor->running())
	{
		autoSave();
	}

	addComment(getString("compiling"));
	gViewerStats.incStat(LLViewerStats::ST_LSL_SAVE_COUNT);
	mSaveCallback(mUserdata, mCloseAfterSave);
}

void LLScriptEditor::loadFile(const std::string& filename)
{
	if (!filename.empty())
	{
		llifstream file(filename.c_str());
		if (file.is_open())
		{
			mEditor->clear();
			std::string line, text;
			while (!file.eof())
			{
				getline(file, line);
				text += line + "\n";
			}
			file.close();
			LLWString wtext = utf8str_to_wstring(text);
			LLWStringUtil::replaceTabsWithSpaces(wtext, 4);
			text = wstring_to_utf8str(wtext);
			setScriptText(text, true, false);
			enableSave(true);
		}
	}
}

void LLScriptEditor::updateDynamicHelp(bool immediate)
{
	mLastHelpUpdate = gFrameTimeSeconds;

	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater || ! help_floater->getVisible()) return;

	// Update back and forward buttons
	LLButton* fwd_button = help_floater->getChild<LLButton>("fwd_btn");
	LLButton* back_button = help_floater->getChild<LLButton>("back_btn");
	LLMediaCtrl* browser =
		help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
	back_button->setEnabled(browser->canNavigateBack());
	fwd_button->setEnabled(browser->canNavigateForward());

	static LLCachedControl<bool> help_follow_cursor(gSavedSettings,
													"ScriptHelpFollowsCursor");
	help_floater->childSetValue("lock_check", (bool)help_follow_cursor);
	if (!immediate && !help_follow_cursor)
	{
		return;
	}

	const LLTextSegment* segment = NULL;
	std::vector<const LLTextSegment*> selected_segments;
	mEditor->getSelectedSegments(selected_segments);

	// Try segments in selection range first
	for (std::vector<const LLTextSegment*>::iterator
			segment_iter = selected_segments.begin(),
			end = selected_segments.end();
		 segment_iter != end; ++segment_iter)
	{
		if ((*segment_iter)->getToken() &&
			(*segment_iter)->getToken()->getType() == LLKeywordToken::WORD)
		{
			segment = *segment_iter;
			break;
		}
	}

	// Then try previous segment in case we just typed it
	if (!segment)
	{
		const LLTextSegment* test_segment = mEditor->getPreviousSegment();
		if (test_segment->getToken() &&
			test_segment->getToken()->getType() == LLKeywordToken::WORD)
		{
			segment = test_segment;
		}
	}

	if (segment && segment->getToken() != mLastHelpToken)
	{
		mLastHelpToken = segment->getToken();
		// Use Wtext since segment's start/end are made for wstring and will
		// result in a shift for case of multi-byte symbols inside std::string.
		LLWString seg_txt = mEditor->getWText().substr(segment->getStart(),
													   segment->getEnd() -
													   segment->getStart());
		setHelpPage(wstring_to_utf8str(seg_txt));
	}
	else if (immediate)
	{
		setHelpPage(LLStringUtil::null);
	}
}

void LLScriptEditor::setHelpPage(const std::string& help_string)
{
	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater) return;

	LLMediaCtrl* web_browser =
		help_floater->getChild<LLMediaCtrl>("lsl_guide_html");

	LLUIString url_string = gSavedSettings.getString("LSLHelpURL");
	std::string topic = help_string;
	if (topic.empty())
	{
		topic = gSavedSettings.getString("LSLHelpDefaultTopic");
	}
	url_string.setArg("[LSL_STRING]", topic);

	addHelpItemToHistory(help_string);

	web_browser->navigateTo(url_string);
}

void LLScriptEditor::addHelpItemToHistory(const std::string& help_string)
{
	if (help_string.empty()) return;

	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater) return;

	LLComboBox* history_combo =
		help_floater->getChild<LLComboBox>("history_combo");

	// Separate history items from full item list
	if (mLiveHelpHistorySize == 0)
	{
		LLSD row;
		row["columns"][0]["type"] = "separator";
		history_combo->addElement(row, ADD_TOP);
	}
	// Delete all history items over history limit
	while (mLiveHelpHistorySize > MAX_HISTORY_COUNT - 1)
	{
		history_combo->remove(--mLiveHelpHistorySize);
	}

	history_combo->setSimple(help_string);
	S32 index = history_combo->getCurrentIndex();

	// If help string exists in the combo box
	if (index >= 0)
	{
		S32 cur_index = history_combo->getCurrentIndex();
		if (cur_index < mLiveHelpHistorySize)
		{
			// Item found in history, bubble up to top
			history_combo->remove(history_combo->getCurrentIndex());
			--mLiveHelpHistorySize;
		}
	}
	history_combo->add(help_string, LLSD(help_string), ADD_TOP);
	history_combo->selectFirstItem();
	++mLiveHelpHistorySize;
}

bool LLScriptEditor::handleSaveChangesDialog(const LLSD& notification,
											 const LLSD& response)
{
	mSaveDialogShown = false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:  // "Yes"
			// Close after saving
			doSave(true);
			break;

		case 1:  // "No"
			if (!mAutosaveFilename.empty())
			{
				llinfos << "Remove autosave: " << mAutosaveFilename << llendl;
				LLFile::remove(mAutosaveFilename);
			}
			mForceClose = true;
			// This will close immediately because mForceClose is true, so we
			// would not go into an infinite loop with these dialogs. JC
			((LLFloater*) getParent())->close();
			break;

		case 2: // "Cancel"
		default:
			// If we were quitting, we did not really mean it.
			gAppViewerp->abortQuit();
	}

	return false;
}

bool LLScriptEditor::handleReloadFromServerDialog(const LLSD& notification,
												  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 1)
	{
		if (mLoadCallback)
		{
			setScriptText(getString("loading"), false);
			mLoadCallback(mUserdata);
		}
	}
	return false;
}

void LLScriptEditor::selectFirstError()
{
	// Select the first item
	mErrorList->selectFirstItem();
	onErrorList(mErrorList, this);
}

struct LLScriptAssetData
{
	LLScriptEditor* instance;
	LLUUID			item_id;
};

bool LLScriptEditor::loadAsset(LLViewerInventoryItem* item)
{
	if (!gAgent.allowOperation(PERM_COPY, item->getPermissions(),
							   GP_OBJECT_MANIPULATE) ||
		!gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
							   GP_OBJECT_MANIPULATE))
	{
		return false;
	}

	LLScriptAssetData* data = new LLScriptAssetData();
	data->instance = this;
	data->item_id = item->getUUID();
	gAssetStoragep->getInvItemAsset(LLHost(), gAgentID, gAgentSessionID,
									item->getPermissions().getOwner(),
									LLUUID::null, data->item_id,
									item->getAssetUUID(), item->getType(),
									onLoadComplete, (void*)data, true);
	return true;
}

//static
void LLScriptEditor::onLoadComplete(const LLUUID& asset_id,
									LLAssetType::EType type, void* userdata,
									S32 status, LLExtStat)
{
	LLScriptAssetData* data = (LLScriptAssetData*)userdata;
	if (!data) return;

	LLScriptEditor* self = data->instance;
	LLUUID item_id = data->item_id;
	delete data;

	if (!self || !sInstances.count(self)) return;

	LLInventoryItem* item = gInventory.getItem(item_id);
	if (!item)
	{
		llwarns << "Script inventory item " << item_id << " is gone" << llendl;
		return;
	}

	if (status == 0)
	{
		LL_DEBUGS("ScriptEditor") << "Got #include asset Id " << asset_id
								  << " for item Id " << item_id << LL_ENDL;
		// At this point, the asset data has been loaded into the cache
		item->setAssetUUID(asset_id);
		// Resume the preprocessing when paused
		if (self->mPreprocessState == PREPROCESS_WAITING)
		{
			self->mPreprocessState = PREPROCESS_RESUME;
		}
	}
	else if (self->mPreprocessState == PREPROCESS_WAITING)
	{
		LL_DEBUGS("ScriptEditor") << "#include asset Id " << asset_id
								  << " for item Id " << item_id
								  << " not available" << LL_ENDL;
		// Abort the preprocessing when paused
		self->mPreprocessState = PREPROCESS_DONE;
	}
}

//static
LLViewerInventoryItem* LLScriptEditor::getScriptItem(const std::string& name)
{
	if (name.empty() || name == "|" || name[name.length() - 1] == '|')
	{
		llwarns << "Invalid script item inventory name: " << name << llendl;
		return NULL;
	}

	// Split the string into path elements
	std::string item_name = name;
	std::string cat_name;
	std::deque<std::string> path;
	size_t i;
	while ((i = item_name.find('|')) != std::string::npos)
	{
		cat_name = item_name.substr(0, i);
		item_name = item_name.substr(i + 1);
		// cat_name is empty when 2+ successive '|' exist in path, or when one
		// is leading the full path. In both cases, skip the empty element.
		if (!cat_name.empty())
		{
			LL_DEBUGS("ScriptEditor") << "Pushing category name: " << cat_name
									  << LL_ENDL;
			path.emplace_back(cat_name);
		}
	}
	LL_DEBUGS("ScriptEditor") << "Searching for item named: " << item_name
							  << LL_ENDL;

	// Search for the category where the script should be located
	LLUUID cat_id = gInventory.getRootFolderID();
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(cat_id, cats, items);
	while (!path.empty())
	{
		cat_name = path.front();
		path.pop_front();

		LL_DEBUGS("ScriptEditor") << "Searching category named: " << cat_name
								  << " in category " << cat_id << LL_ENDL;

		// Search for next category down the path
		LLViewerInventoryCategory* cat = NULL;
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			cat = (*cats)[i];
			if (cat && cat->getName() == cat_name)
			{
				cat_id = cat->getUUID();
				LL_DEBUGS("ScriptEditor") << "Found category " << cat_id
										  << LL_ENDL;

				break;
			}
			cat = NULL;
		}
		if (!cat)
		{
			LL_DEBUGS("ScriptEditor") << "Category " << cat_name
									  << " not found" << LL_ENDL;
			// Next category in path not found...
			return NULL;
		}

		gInventory.getDirectDescendentsOf(cat_id, cats, items);
	}

	LL_DEBUGS("ScriptEditor") << "Searching for item named: " << item_name
							  << " in category " << cat_id << LL_ENDL;

	// We reached the deepest category, and should find the script here
	for (S32 i = 0, count = items->size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = (*items)[i];
		if (item && item->getType() == LLAssetType::AT_LSL_TEXT &&
			item->getName() == item_name)				
		{
			return item;
		}
	}

	return NULL;
}

//static
S32 LLScriptEditor::loadInclude(std::string& include_name,
								const std::string& path, std::string& buffer,
								void* userdata)
{
	buffer.clear();

	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self)
	{
		return HBPreprocessor::FAILURE;
	}

	std::string item_path;
	// Check whether we want to include files from the file system instead of
	// script assets from the inventory
	bool in_home_dir = path.find("~/") == 0;
	if (in_home_dir || path.find("./") == 0)
	{
		LL_DEBUGS("ScriptEditor") << "Including from file system with path: "
								  << path << LL_ENDL;
		if (in_home_dir)
		{
			// Search in user "home" directory, without fallback sub-directory
			item_path = gDirUtilp->getUserFilename(path, "", include_name);
		}
		else
		{
			item_path = gDirUtilp->getUserFilename(path, "include",
												   include_name);
		}
		if (item_path.empty())
		{
			LL_DEBUGS("ScriptEditor") << "File not found" << LL_ENDL;
			return HBPreprocessor::FAILURE;
		}
		LL_DEBUGS("ScriptEditor") << "File found: " << item_path << LL_ENDL;

		llifstream include_file(item_path.c_str());
		if (!include_file.is_open())
		{
			llwarns << "Failure to open file: " << item_path << llendl;
			return HBPreprocessor::FAILURE;
		}

		// Return the full path of the include file we opened successfully
		include_name = item_path;
		self->addComment(self->getString("including_file") + " " + item_path);

		std::string line;
		while (!include_file.eof())
		{
			getline(include_file, line);
			buffer += line + "\n";
		}
		include_file.close();

		return HBPreprocessor::SUCCESS;
	}

	// Get item current path in inventory
	item_path = self->getItemPath();

	std::string real_path;
	LLViewerInventoryItem* item = NULL;
	if (!path.empty())	// check any path set with #pragma include-from:
	{
		real_path = path;
		if (path.back() != '|')
		{
			// Add a separator at the end when missing
			real_path += "|";
		}
		if (path.front() != '|')
		{
			// This is a relative path
			if (item_path.empty())
			{
				// But with an empty item path, it is relative to the inventory
				// root ...
				real_path = "|" + real_path;
			}
			else
			{
				real_path = item_path + real_path;
			}
		}
		LL_DEBUGS("ScriptEditor") << "Searching for inventory item "
								  << include_name << " in inventory folder: "
								  << real_path << LL_ENDL;
		item = getScriptItem(real_path + include_name);
	}
	if (!item && !item_path.empty())
	{
		// Retry with the item folder
		real_path = item_path;
		LL_DEBUGS("ScriptEditor") << "Searching for inventory item "
								  << include_name << " in inventory folder: "
								  << real_path << LL_ENDL;
		item = getScriptItem(real_path + include_name);
	}
	if (!item && item_path != "|Scripts|")
	{
		// Retry with the Scripts folder
		real_path = "|Scripts|";
		LL_DEBUGS("ScriptEditor") << "Searching for inventory item "
								  << include_name << " in inventory folder: "
								  << real_path << LL_ENDL;
		item = getScriptItem(real_path + include_name);
	}
	if (!item)
	{
		LL_DEBUGS("ScriptEditor") << "Item for #include " << include_name
								  << " not found" << LL_ENDL;
		return HBPreprocessor::FAILURE;
	}

	// asset_id is LLUUID::null unless it just got fetched and we are actually
	// in a HBPreprocessor::resume() call.
	const LLUUID& asset_id = item->getAssetUUID();
	if (asset_id.notNull())
	{
		// Try and find the asset in the cache
		LLFileSystem file(asset_id);
		S32 file_length = file.getSize();
		if (file_length > 0)
		{
			// Get the asset data (the included script text)
			char* data = new char[file_length + 1];
			file.read((U8*)data, file_length);
			data[file_length] = 0;
			buffer.assign(&data[0]);
			delete[] data;
			// If it is an escaped include script, convert it to its
			// non-escaped version.
			if (buffer.find(ESCAPED_INCLUDE_MARKER) != std::string::npos)
			{
				buffer = getIncludeSources(buffer);
			}
			// If it is a preprocessed script, remove the escaped sources
			else if (buffer.find(ESCAPED_SOURCES_MARKER) != std::string::npos)
			{
				buffer = removeEscapedSources(buffer);
			}
			// Remove the asset data from the cache to ensure that it will be
			// re-fetched next time and kept up to date with any change.
			LLFileSystem::removeFile(asset_id);
			// And reset the asset UUID for this inventory item.
			item->setAssetUUID(LLUUID::null);

			return HBPreprocessor::SUCCESS;
		}
	}

	self->addComment(self->getString("including_script") + " " + real_path +
					 include_name);
	self->loadAsset(item);
	return HBPreprocessor::PAUSED;
}

//static
void LLScriptEditor::preprocessorMessage(const std::string& message,
										 bool is_warning, void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		self->addComment(message, !is_warning);
	}
}

//static
void LLScriptEditor::onIdle(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		self->preprocess();
	}
}

//static
std::string LLScriptEditor::escapeSources(const std::string& sources)
{
	size_t len = sources.length();
	if (!len)
	{
		return "";
	}

	std::string result = "\n" + ESCAPED_SOURCES_MARKER;
	size_t pos = 0;
	while (pos < len)
	{
		result += ESCAPE_STRING + get_one_line(sources, pos);
	}

	len = result.length();
	if (result[len - 1] != '\n')
	{
		result += '\n';
	}

	return result;
}

//static
std::string LLScriptEditor::unescapeSources(const std::string& sources)
{
	size_t len = sources.length();
	if (!len)
	{
		return "";
	}

	std::string result, line;
	size_t pos = 0;
	while (pos < len && get_one_line(sources, pos) != ESCAPED_SOURCES_MARKER);

	size_t esc_len = ESCAPE_STRING.length();
	while (pos < len)
	{
		line = get_one_line(sources, pos);
		if (line.find(ESCAPE_STRING) != 0)
		{
			break;
		}
		result += line.substr(esc_len);
	}

	len = result.length();
	if (result[len - 1] != '\n')
	{
		result += '\n';
	}

	return result;
}

//static
std::string LLScriptEditor::removeEscapedSources(const std::string& sources)
{
	size_t len = sources.length();
	if (!len)
	{
		return "";
	}

	std::string result, line;
	size_t pos = 0;
	while (pos < len)
	{
		line = get_one_line(sources, pos);
		if (line == ESCAPED_SOURCES_MARKER)
		{
			break;
		}
		result += line;
	}

	len = result.length();
	if (result[len - 1] != '\n')
	{
		result += '\n';
	}

	return result;
}

//static
std::string LLScriptEditor::setIncludeSources(const std::string& sources)
{
	size_t len = sources.length();
	if (!len)
	{
		return "";
	}

	std::string result = ESCAPED_INCLUDE_MARKER;
	size_t pos = 0;
	while (pos < len)
	{
		result += ESCAPE_STRING + get_one_line(sources, pos);
	}

	len = result.length();
	if (result[len - 1] != '\n')
	{
		result += '\n';
	}

	return result + ESCAPED_INCLUDE_FOOTER + DUMMY_STATE;
}

//static
std::string LLScriptEditor::getIncludeSources(const std::string& sources)
{
	size_t len = sources.length();
	if (!len)
	{
		return "";
	}

	std::string result, line;
	size_t pos = 0;
	while (pos < len &&
		   get_one_line(sources, pos) != ESCAPED_INCLUDE_MARKER);

	size_t esc_len = ESCAPE_STRING.length();
	while (pos < len)
	{
		line = get_one_line(sources, pos);
		if (line.find(ESCAPE_STRING) != 0)
		{
			break;
		}
		result += line.substr(esc_len);
	}

	len = result.length();
	if (result[len - 1] != '\n')
	{
		result += '\n';
	}

	return result;
}

//static
std::string LLScriptEditor::convertSources(const std::string& sources)
{
	static size_t start_len = ALIEN_ESCAPED_START_MARKER.length();

	size_t pos = sources.find(ALIEN_ESCAPED_START_MARKER);
	if (pos == std::string::npos)
	{
		return sources;
	}

	std::string result = sources.substr(pos + start_len);
	pos = result.find(ALIEN_ESCAPED_END_MARKER);
	if (pos == std::string::npos)
	{
		llwarns << "Missing marker for end of preprocessed source in script text"
				<< llendl;
	}
	else
	{
		result = result.substr(0, pos);
	}

	pos = result.length();
	if (pos == 0 || result[pos - 1] != '\n')
	{
		result += "\n";
	}

	// Unescape comments
	LLStringUtil::replaceString(result, "/|/", "//");
	LLStringUtil::replaceString(result, "/|*", "/*");
	LLStringUtil::replaceString(result, "*|/", "*/");
	// Also convert special defines
	LLStringUtil::replaceString(result, "__AGENTID__", "__AGENT_ID__");
	LLStringUtil::replaceString(result, "__AGENTKEY__", "__AGENT_ID__");
	LLStringUtil::replaceString(result, "__AGENTNAME__", "__AGENT_NAME__");
	// Approximatively equivalent
	LLStringUtil::replaceString(result, "__AGENTIDRAW__", "__AGENT_ID__");
	LLStringUtil::replaceString(result, "__SHORTFILE__", "__FILE__");

	return result;
}

//static
bool LLScriptEditor::onHelpWebDialog(const LLSD& notification,
									 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL(notification["payload"]["help_url"]);
	}

	return false;
}

//static
void LLScriptEditor::onBtnHelp(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		LLSD payload;
		payload["help_url"] = LSL_DOC_URL;
		gNotifications.add("WebLaunchLSLGuide", LLSD(), payload,
						   onHelpWebDialog);
	}
}

//static
void LLScriptEditor::onBtnDynamicHelp(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	LLFloater* floater = self->mLiveHelpHandle.get();
	if (floater)
	{
		floater->setFocus(true);
		self->updateDynamicHelp(true);
		return;
	}

	floater = new LLFloater("lsl help");
	LLUICtrlFactory::getInstance()->buildFloater(floater,
												 "floater_lsl_guide.xml");
	((LLFloater*)self->getParent())->addDependentFloater(floater);
	floater->childSetCommitCallback("lock_check", onCheckLock, userdata);
	floater->childSetValue("lock_check",
						   gSavedSettings.getBool("ScriptHelpFollowsCursor"));
	floater->childSetCommitCallback("history_combo", onHelpComboCommit,
									userdata);
	floater->childSetAction("back_btn", onClickBack, userdata);
	floater->childSetAction("fwd_btn", onClickForward, userdata);

	LLMediaCtrl* browser = floater->getChild<LLMediaCtrl>("lsl_guide_html");
	browser->setAlwaysRefresh(true);

	LLColor3 color = LLColor3(gColors.getColor("LslPreprocessorTextFgColor"));
	LLComboBox* help_combo = floater->getChild<LLComboBox>("history_combo");
	for (LLKeywords::keyword_iterator_t it = self->mEditor->keywordsBegin(),
										end = self->mEditor->keywordsEnd();
		 it != end; ++it)
	{
		LLKeywordToken* token = it->second;
		// *HACK: do not register preprocessor directives or macros/defines
		if (token && token->getColor() != color)
		{
			help_combo->add(wstring_to_utf8str(token->getToken()));
		}
	}
	help_combo->sortByName();

	// Re-initialize help variables
	self->mLastHelpToken = NULL;
	self->mLiveHelpHandle = floater->getHandle();
	self->mLiveHelpHistorySize = 0;
	self->updateDynamicHelp(true);
}

//static
void LLScriptEditor::onHelpFollowCursor(void*)
{
	gSavedSettings.setBool("ScriptHelpFollowsCursor",
						   !gSavedSettings.getBool("ScriptHelpFollowsCursor"));
}

//static
void LLScriptEditor::onClickBack(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	LLFloater* floater = self->mLiveHelpHandle.get();
	if (floater)
	{
		LLMediaCtrl* browserp =
			floater->getChild<LLMediaCtrl>("lsl_guide_html");
		if (browserp)
		{
			browserp->navigateBack();
		}
	}
}

//static
void LLScriptEditor::onClickForward(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	LLFloater* floater = self->mLiveHelpHandle.get();
	if (floater)
	{
		LLMediaCtrl* browserp =
			floater->getChild<LLMediaCtrl>("lsl_guide_html");
		if (browserp)
		{
			browserp->navigateForward();
		}
	}
}

//static
void LLScriptEditor::onCheckLock(LLUICtrl* ctrl, void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		// Clear out token any time we lock the frame, so we will refresh web
		// page immediately when unlocked
		gSavedSettings.setBool("ScriptHelpFollowsCursor",
							   ctrl->getValue().asBoolean());

		self->mLastHelpToken = NULL;
	}
}

//static
void LLScriptEditor::onHelpComboCommit(LLUICtrl* ctrl, void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;

	LLFloater* floater = self->mLiveHelpHandle.get();
	if (floater)
	{
		std::string help_string = ctrl->getValue().asString();
		self->addHelpItemToHistory(help_string);

		LLMediaCtrl* web_browser =
			floater->getChild<LLMediaCtrl>("lsl_guide_html");
		LLUIString url_string = gSavedSettings.getString("LSLHelpURL");
		url_string.setArg("[LSL_STRING]", help_string);
		web_browser->navigateTo(url_string);
	}
}

//static
void LLScriptEditor::onBtnInsertFunction(LLUICtrl*, void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	// Insert sample code
	if (self->mEditor->getEnabled())
	{
		self->mEditor->insertText(self->mFunctions->getSimple());
	}
	self->mEditor->setFocus(true);
	self->setHelpPage(self->mFunctions->getSimple());
}

//static
bool LLScriptEditor::enableLoadFile(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return !HBFileSelector::isInUse() && self && self->mHasScriptData && 
		   !self->mIsSaving &&
		   self->mTabContainer->getCurrentPanelIndex() == 0;
}

//static
bool LLScriptEditor::enableSaveFile(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return !HBFileSelector::isInUse() && self && self->mHasScriptData &&
		   !self->mIsSaving;
}

//static
bool LLScriptEditor::enableRaw(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mHasScriptData && !self->mIsSaving &&
		   self->mTabContainer->getCurrentPanelIndex() == 0;
}

//static
bool LLScriptEditor::enableCallback(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->hasChanged() && !self->mIsSaving;
}

//static
void LLScriptEditor::loadFromFileCallback(HBFileSelector::ELoadFilter type,
										  std::string& filename,
										  void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && sInstances.count(self))
	{
		self->loadFile(filename);
	}
	else
	{
		gNotifications.add("LoadScriptAborted");
	}
}

//static
void LLScriptEditor::onBtnLoadFromFile(void* userdata)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_SCRIPT,
							 loadFromFileCallback, userdata);
}

struct LLSaveToFileData
{
	LLScriptEditor*	instance;
	std::string		sources;
};

//static
void LLScriptEditor::saveToFileCallback(HBFileSelector::ESaveFilter type,
										std::string& filename,
										void* userdata)
{
	LLSaveToFileData* data = (LLSaveToFileData*)userdata;
	if (!data) return;	// Paranoia

	LLScriptEditor* self = data->instance;
	if (!self || !sInstances.count(self))
	{
		delete data;
		gNotifications.add("SaveScriptAborted");
		return;
	}

	if (!filename.empty())
	{
		std::string lcname = filename;
		LLStringUtil::toLower(lcname);
		if (lcname.find(".lsl") != lcname.length() - 4 &&
			lcname.find(".txt") != lcname.length() - 4)
		{
			filename += ".lsl";
		}
		llofstream file(filename.c_str());
		if (file.is_open())
		{
			file << data->sources;
			file.close();
		}
	}

	delete data;
}

//static
void LLScriptEditor::onBtnSaveToFile(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && sInstances.count(self))
	{
		S32 active_tab = self->mTabContainer->getCurrentPanelIndex();
		LLSaveToFileData* data = new LLSaveToFileData();
		data->instance = self;
		data->sources = active_tab == 0 ? self->mEditor->getText()
										: self->mSavedSources->getText();
		std::string suggestion = self->mScriptName + ".lsl";
		HBFileSelector::saveFile(HBFileSelector::FFSAVE_LSL, suggestion,
								 saveToFileCallback, data);
	}
}

//static
void LLScriptEditor::onEditedFileChanged(const std::string& filename,
										 void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && sInstances.count(self))
	{
		if (filename == self->mAutosaveFilename)
		{
			self->loadFile(filename);
		}
		else
		{
			llwarns << "Watched file (" << filename
					<< ") and auto-saved file (" << self->mAutosaveFilename
					<< ") do not match !" << llendl;
		}
	}
}

//static
void LLScriptEditor::onEditExternal(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && sInstances.count(self))
	{
		self->autoSave();
		if (self->mExternalEditor)
		{
			self->mExternalEditor->kill();
		}
		else
		{
			self->mExternalEditor = new HBExternalEditor(onEditedFileChanged,
														 self);
		}
		if (!self->mExternalEditor->open(self->mAutosaveFilename))
		{
			self->addComment(self->mExternalEditor->getErrorMessage(), true);
		}
	}
}

//static
void LLScriptEditor::onEditRaw(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && sInstances.count(self))
	{
		self->setEditedTextFromSaved();
	}
}

//static
void LLScriptEditor::onBtnSave(void* userdata)
{
	onFlyoutBtnSave(NULL, userdata);
}

//static
void LLScriptEditor::onFlyoutBtnSave(LLUICtrl* ctrl, void* userdata)
{
	// Do the save, but do not close afterwards
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	if (ctrl && ctrl->getValue().asString() == "save_include")
	{
		self->mSavedSources->setText(setIncludeSources(self->mEditor->getText()));
		self->doSave(false, false);
	}
	else
	{
		self->doSave(false);
	}
}

//static
void LLScriptEditor::onBtnUndoChanges(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && !self->mEditor->tryToRevertToPristineState())
	{
		gNotifications.add("ScriptCannotUndo", LLSD(), LLSD(),
						   boost::bind(&LLScriptEditor::handleReloadFromServerDialog,
									   self, _1, _2));
	}
}

//static
void LLScriptEditor::onSearchMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		S32 active_tab = self->mTabContainer->getCurrentPanelIndex();
		LLFloaterSearchReplace::show(active_tab == 0 ? self->mEditor
													 : self->mSavedSources);
	}
}

//static
void LLScriptEditor::onUndoMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && self->mTabContainer->getCurrentPanelIndex() == 0)
	{
		self->mEditor->undo();
	}
}

//static
void LLScriptEditor::onRedoMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && self->mTabContainer->getCurrentPanelIndex() == 0)
	{
		self->mEditor->redo();
	}
}

//static
void LLScriptEditor::onCutMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && self->mTabContainer->getCurrentPanelIndex() == 0)
	{
		self->mEditor->cut();
	}
}

//static
void LLScriptEditor::onCopyMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	if (currrent_tab == 0)
	{
		self->mEditor->copy();
	}
	else
	{
		self->mSavedSources->copy();
	}
}

//static
void LLScriptEditor::onPasteMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self && self->mTabContainer->getCurrentPanelIndex() == 0)
	{
		self->mEditor->paste();
	}
}

//static
void LLScriptEditor::onSelectAllMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	if (currrent_tab == 0)
	{
		self->mEditor->selectAll();
	}
	else
	{
		self->mSavedSources->selectAll();
	}
}

//static
void LLScriptEditor::onDeselectMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	if (currrent_tab == 0)
	{
		self->mEditor->deselect();
	}
	else
	{
		self->mSavedSources->deselect();
	}
}

//static
bool LLScriptEditor::enableUndoMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mTabContainer->getCurrentPanelIndex() == 0 &&
		   self->mEditor->canUndo();
}

//static
bool LLScriptEditor::enableRedoMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mTabContainer->getCurrentPanelIndex() == 0 &&
		   self->mEditor->canRedo();
}

//static
bool LLScriptEditor::enableCutMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mTabContainer->getCurrentPanelIndex() == 0 &&
		   self->mEditor->canCut();
}

//static
bool LLScriptEditor::enableCopyMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return false;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	return (currrent_tab == 0 && self->mEditor->canCopy()) ||
		   (currrent_tab == 1 && self->mSavedSources->canCopy());
}

//static
bool LLScriptEditor::enablePasteMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mTabContainer->getCurrentPanelIndex() == 0 &&
		   self->mEditor->canPaste();
}

//static
bool LLScriptEditor::enableSelectAllMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return false;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	return (currrent_tab == 0 && self->mEditor->canSelectAll()) ||
		   (currrent_tab == 1 && self->mSavedSources->canSelectAll());
}

//static
bool LLScriptEditor::enableDeselectMenu(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (!self) return false;

	S32 currrent_tab = self->mTabContainer->getCurrentPanelIndex();
	return (currrent_tab == 0 && self->mEditor->canDeselect()) ||
		   (currrent_tab == 1 && self->mSavedSources->canDeselect());
}

//static
bool LLScriptEditor::enableHelp(void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	return self && self->mTabContainer->getCurrentPanelIndex() == 0;
}

//static
void LLScriptEditor::onErrorList(LLUICtrl*, void* user_data)
{
	LLScriptEditor* self = (LLScriptEditor*)user_data;
	if (!self) return;

	LLScrollListItem* item = self->mErrorList->getFirstSelected();
	if (item)
	{
		// *FIXME: This fucked up little hack is here because we do not have a
		// grep library. This is very brittle code.
		const LLScrollListCell* cell = item->getColumn(0);
		std::string text = cell->getValue().asString();
		text.erase(0, 1);
		LLStringUtil::replaceChar(text, ',',' ');
		LLStringUtil::replaceChar(text, ')',' ');
		S32 row = 0;
		S32 column = 0;
		if (sscanf(text.c_str(), "%d %d", &row, &column) <= 0)
		{
			// Not an error with row/column indicator: abort now.
			return;
		}

		// The row and column do always map to the saved sources.
		self->mSavedSources->setCursor(row, column);
		// Make it obvious to the user despite the lack of a cursor in a
		// disabled text editor
		S32 pos = self->mSavedSources->getCursorPos();
		self->mSavedSources->setSelection(pos, pos + 1);

		// If the sources have been preprocessed, then the compilation error
		// line is likely not the one that was reported in the message and we
		// need to find the corresponding line in the original non-preprocessed
		// source. HBPreprocessor provides this facility.
		// NOTE: the column number might also be invalid, if the line contained
		// a #defined symbol, but we cannot track such changes as easily...
		if (self->mPreprocessor)
		{
			// NOTE: the script editor first line is row 0, while the
			// preprocessor counts from line 1 upwards.
			S32 line = self->mPreprocessor->getOriginalLine(row + 1);
			if (line > 0)
			{
				row = line - 1;
			}
		}

		self->mEditor->setCursor(row, column);
		self->mEditor->setFocus(true);
	}
}

//static
void LLScriptEditor::onMonoCheckboxClicked(LLUICtrl*, void* userdata)
{
	LLScriptEditor* self = (LLScriptEditor*)userdata;
	if (self)
	{
		self->enableSave(true);
	}
}

//static
void LLScriptEditor::loadFunctions(const std::string& filename)
{
	std::string filepath =
		gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, filename);
	if (!LLFile::isfile(filepath))
	{
		llwarns << "Failed to load LSL functions table from: " << filename
				<< " - File does not exist ! " << llendl;
		return;
	}

	llifstream importer(filepath.c_str());
	if (!importer.is_open())
	{
		llwarns << "Failed to load LSL functions table from: " << filename
				<< " - Could not open and read that file ! " << llendl;
		return;
	}
	LLSD function_list;
	LLSDSerialize::fromXMLDocument(function_list, importer);
	importer.close();

	for (LLSD::map_const_iterator it = function_list.beginMap(),
								  end = function_list.endMap();
		 it != end; ++it)
	{
		sParsedFunctions.emplace_back(it->first,
									  it->second["tooltip"].asString(),
									  it->second["sleep_time"].asReal(),
									  it->second["god_only"].asBoolean());
	}

	llinfos << "Loaded LSL functions table from: " << filename << llendl;
}

// ----------------------------------------------------------------------------
// LLPreviewScript class
// ----------------------------------------------------------------------------

// Wrapper method, to avoid having to expose LLScriptEditor class definition
// in llpreviewscript.h just for a couple of calls in llstartup.cpp...
void LLPreviewScript::loadFunctions(const std::string& filename)
{
	LLScriptEditor::loadFunctions(filename);
}

// Wrapper method to set the custom font for LLScriptEditor. Called from
// LLViewerWindow::initFonts() (i.e. after the fonts system has been properly
// initialized), and from llviewercontrol.cpp on setting change.
//static
void LLPreviewScript::refreshCachedSettings()
{
	std::string font_name = gSavedSettings.getString("ScriptEditorFont");
	if (font_name.empty())
	{
		LLScriptEditor::sCustomFont = NULL;
	}
	else
	{
		LLScriptEditor::sCustomFont = LLFontGL::getFont(font_name.c_str());
	}
}

//static
void* LLPreviewScript::createScriptEdPanel(void* userdata)
{
	LLPreviewScript* self = (LLPreviewScript*)userdata;
	self->mScriptEd = new LLScriptEditor(self->mItemUUID, onLoad, onSave,
										 onSearchReplace, self);
	return self->mScriptEd;
}

LLPreviewScript::LLPreviewScript(const std::string& name, const LLRect& rect,
								 const std::string& title,
								 const LLUUID& item_id)
:	LLPreview(name, rect, title, item_id, LLUUID::null, true,
			  SCRIPT_MIN_WIDTH, SCRIPT_MIN_HEIGHT)
{
	LLRect cur_rect = rect;

	LLCallbackMap::map_t factory_map;
	factory_map["script panel"] = LLCallbackMap(createScriptEdPanel, this);

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_script_preview.xml",
												 &factory_map);

	const LLInventoryItem* item = getItem();

	childSetCommitCallback("desc", LLPreview::onText, this);
	childSetText("desc", item->getDescription());
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);

	LLCheckBoxCtrl* mono_check = mScriptEd->getMonoCheckBox();
	bool use_mono = gIsInSecondLife && have_script_upload_cap(LLUUID::null);
	mono_check->setEnabled(use_mono);
	mono_check->set(use_mono);

	if (!getFloaterHost() && !getHost() &&
		getAssetStatus() == PREVIEW_ASSET_UNLOADED)
	{
		loadAsset();
	}

	setTitle(title);
	mScriptEd->setScriptName(title);

	if (!getHost())
	{
		reshape(cur_rect.getWidth(), cur_rect.getHeight());
		setRect(cur_rect);
	}
}

LLTextEditor* LLPreviewScript::getEditor()
{
	return mScriptEd->mEditor;
}

void LLPreviewScript::callbackLSLCompileSucceeded()
{
	llinfos << "LSL byte-code saved" << llendl;
	mScriptEd->addComment(getString("compile_success"));
	mScriptEd->addComment(getString("save_complete"));
	mScriptEd->enableEdit(true);
	closeIfNeeded();
}

void LLPreviewScript::callbackLSLCompileFailed(const LLSD& compile_errors)
{
	llwarns << "Compile failed !" << llendl;

	for (LLSD::array_const_iterator line = compile_errors.beginArray();
		 line < compile_errors.endArray(); ++line)
	{
		std::string error_message = line->asString();
		LLStringUtil::stripNonprintable(error_message);
		mScriptEd->addComment(error_message, true);
	}
	mScriptEd->selectFirstError();
	mScriptEd->enableEdit(true);
	closeIfNeeded();
}

void LLPreviewScript::loadAsset()
{
	// *HACK: we poke into inventory to see if it is there, and if so, then it
	// might be part of the inventory library. If it is in the library, then
	// you can see the script, but not modify it.
	const LLInventoryItem* item = gInventory.getItem(mItemUUID);
	if (!item)
	{
		// Do the more generic search.
		getItem();
	}
	if (!item)
	{
		mScriptEd->setScriptText(HELLO_LSL, true);
		mAssetStatus = PREVIEW_ASSET_LOADED;
		return;
	}

	bool is_library =
		!gInventory.isObjectDescendentOf(mItemUUID,
										 gInventory.getRootFolderID());
	bool is_copyable = gAgent.allowOperation(PERM_COPY, item->getPermissions(),
											 GP_OBJECT_MANIPULATE);
	bool is_modifiable = gAgent.allowOperation(PERM_MODIFY,
											   item->getPermissions(),
											   GP_OBJECT_MANIPULATE);

	mScriptEd->setScriptName(item->getName());

	if (gAgent.isGodlike() || (is_copyable && (is_modifiable || is_library)))
	{
		LLUUID* new_uuid = new LLUUID(mItemUUID);
		gAssetStoragep->getInvItemAsset(LLHost(), gAgentID, gAgentSessionID,
										item->getPermissions().getOwner(),
										LLUUID::null, item->getUUID(),
										item->getAssetUUID(), item->getType(),
										onLoadComplete, (void*)new_uuid, true);
		mAssetStatus = PREVIEW_ASSET_LOADING;
	}
	else
	{
		mScriptEd->setScriptText(mScriptEd->getString("can_not_view"), false);
		mScriptEd->mEditor->makePristine();
		mScriptEd->mEditor->setEnabled(false);
		mScriptEd->mFunctions->setEnabled(false);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}

	childSetVisible("lock", !is_modifiable);
	mScriptEd->mFunctions->setEnabled(is_modifiable);
}

bool LLPreviewScript::canClose()
{
	return mScriptEd->canClose();
}

void LLPreviewScript::closeIfNeeded()
{
	// Find our window and close it if requested.
	gWindowp->decBusyCount();
	if (mCloseAfterSave)
	{
		if (!mScriptEd->mAutosaveFilename.empty())
		{
			llinfos << "Remove autosave: " << mScriptEd->mAutosaveFilename
					<< llendl;
			LLFile::remove(mScriptEd->mAutosaveFilename);
		}
		close();
	}
}

// Overrides the LLPreview open which attempts to load asset (since we did it
// already)
void LLPreviewScript::open()
{
	LLFloater::open();
}

//static
void LLPreviewScript::onSearchReplace(void* userdata)
{
	LLPreviewScript* self = (LLPreviewScript*)userdata;
	if (!self) return;

	LLScriptEditor* sed = self->mScriptEd;
	if (sed)
	{
		LLFloaterSearchReplace::show(sed->mEditor);
	}
}

//static
void LLPreviewScript::onLoad(void* userdata)
{
	LLPreviewScript* self = (LLPreviewScript*)userdata;
	if (self)
	{
		self->loadAsset();
	}
}

//static
void LLPreviewScript::onSave(void* userdata, bool close_after_save)
{
	LLPreviewScript* self = (LLPreviewScript*)userdata;
	if (self)
	{
		self->mCloseAfterSave = close_after_save;
		self->saveIfNeeded();
	}
}

//static
void LLPreviewScript::finishLSLUpload(LLUUID item_id, LLSD response)
{
	// Find our window and close it if requested.
	LLPreviewScript* self = LLPreviewScript::getInstance(item_id);
	if (self)
	{
		// Bytecode save completed
		if (response.has("compiled") && response["compiled"])
		{
			self->callbackLSLCompileSucceeded();
		}
		else
		{
			self->callbackLSLCompileFailed(response["errors"]);
		}
	}
}

//static
void LLPreviewScript::failedLSLUpload(LLUUID item_id, std::string reason)
{
	// Find our window and close it if requested.
	LLPreviewScript* self = LLPreviewScript::getInstance(item_id);
	if (self)
	{
		LLSD errors;
		errors.append(LLTrans::getString("AssetUploadFailed") + reason);
		self->callbackLSLCompileFailed(errors);
	}
}

// Save needs to compile the text in the buffer. If the compile succeeds, then
// save both assets out to the database. If the compile fails, go ahead and
// save the text anyway so that the user does not get too fucked.
void LLPreviewScript::saveIfNeeded()
{
	if (!mScriptEd->hasChanged())
	{
		return;
	}

	const LLInventoryItem* inv_item = getItem();
	if (!inv_item)
	{
		llwarns << "Missing inventory item: " << mItemUUID << llendl;
		return;
	}

	// Save it out to asset server
	const std::string& url = gAgent.getRegionCapability("UpdateScriptAgent");
	if (url.empty())
	{
		LLSD args;
		args["REASON"] = "missing UpdateScriptAgent capability";
		gNotifications.add("SaveScriptFailReason", args);
		return;
	}

	std::string buffer = mScriptEd->mSavedSources->getText();
	if (buffer.empty())
	{
		llwarns << "Empty or invalid script sources." << llendl;
		return;
	}

	mScriptEd->mEditor->makePristine();
	mScriptEd->enableEdit(false);
	mScriptEd->enableSave(false);

	gWindowp->incBusyCount();

	LLBufferedAssetUploadInfo::inv_uploaded_cb_t proc_ok =
		boost::bind(&LLPreviewScript::finishLSLUpload, _1, _4);

	LLBufferedAssetUploadInfo::failed_cb_t proc_ko =
		boost::bind(&LLPreviewScript::failedLSLUpload, _1, _4);

	bool mono_checked = mScriptEd->monoChecked();
	LLScriptAssetUpload::TargetType_t type;
	if (!gIsInSecondLife || mono_checked)
	{
		type = LLScriptAssetUpload::MONO;
	}
	else
	{
		type = LLScriptAssetUpload::LSL2;
	}

	LLResourceUploadInfo::ptr_t info(new LLScriptAssetUpload(mItemUUID, buffer,
															 type, proc_ok,
															 proc_ko));
	LLViewerAssetUpload::enqueueInventoryUpload(url, info);
}

//static
void LLPreviewScript::onLoadComplete(const LLUUID& asset_id,
									 LLAssetType::EType type, void* user_data,
									 S32 status, LLExtStat)
{
	LL_DEBUGS("ScriptEditor") << "Got uuid " << asset_id << LL_ENDL;
	LLUUID* item_uuid = (LLUUID*)user_data;
	LLPreviewScript* preview = LLPreviewScript::getInstance(*item_uuid);
	if (!preview)
	{
		delete item_uuid;
		return;
	}

	if (status == 0)
	{
		// Get the script text
		LLFileSystem file(asset_id);
		S32 file_length = file.getSize();
		char* buffer = new char[file_length + 1];
		file.read((U8*)buffer, file_length);
		// Put a EOS at the end
		buffer[file_length] = 0;
		preview->mScriptEd->setScriptText(std::string(&buffer[0]), true);
		preview->mScriptEd->mEditor->makePristine();
		delete[] buffer;

		bool is_modifiable = false;
		LLInventoryItem* item = gInventory.getItem(*item_uuid);
		if (item)
		{
			if (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
				   					  GP_OBJECT_MANIPULATE))
			{
				is_modifiable = true;
			}
		}
		preview->mScriptEd->mEditor->setEnabled(is_modifiable);
		preview->mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	else
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

		if (status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE ||
			status == LL_ERR_FILE_EMPTY)
		{
			gNotifications.add("ScriptMissing");
		}
		else if (status == LL_ERR_INSUFFICIENT_PERMISSIONS)
		{
			gNotifications.add("ScriptNoPermissions");
		}
		else
		{
			gNotifications.add("UnableToLoadScript");
		}

		preview->mAssetStatus = PREVIEW_ASSET_ERROR;
		llwarns << "Problem loading script " << *item_uuid << ": status = "
				<< status << llendl;
	}
	delete item_uuid;
}

//static
LLPreviewScript* LLPreviewScript::getInstance(const LLUUID& item_uuid)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator it = LLPreview::sInstances.find(item_uuid);
	if (it != LLPreview::sInstances.end())
	{
		instance = it->second;
	}
	return (LLPreviewScript*)instance;
}

void LLPreviewScript::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPreview::reshape(width, height, called_from_parent);

	if (!isMinimized())
	{
		// So that next time you open a script it will have the same height and
		// width (although not the same position).
		gSavedSettings.setRect("PreviewScriptRect", getRect());
	}
}

// ----------------------------------------------------------------------------
// LLLiveLSLEditor class
// ----------------------------------------------------------------------------

LLLiveLSLEditor::instances_map_t LLLiveLSLEditor::sInstances;

//static
LLLiveLSLEditor* LLLiveLSLEditor::show(const LLUUID& script_id,
									   const LLUUID& object_id)
{
	LLLiveLSLEditor* self = NULL;
	LLUUID xored_id = script_id ^ object_id;
	instances_map_t::iterator it = sInstances.find(xored_id);
	if (it != sInstances.end())
	{
		// Move the existing view to the front
		self = it->second;
		self->open();
	}
	return self;
}

//static
void LLLiveLSLEditor::hide(const LLUUID& script_id, const LLUUID& object_id)
{
	LLUUID xored_id = script_id ^ object_id;
	instances_map_t::iterator it = sInstances.find(xored_id);
	if (it != sInstances.end())
	{
		LLLiveLSLEditor* self = it->second;
		if (self->getParent())
		{
			self->getParent()->removeChild(self);
		}
		delete self;
	}
}

//static
LLLiveLSLEditor* LLLiveLSLEditor::find(const LLUUID& script_id,
									   const LLUUID& object_id)
{
	LLUUID xored_id = script_id ^ object_id;
	return get_ptr_in_map(sInstances, xored_id);
}

//static
void* LLLiveLSLEditor::createScriptEdPanel(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	self->mScriptEd = new LLScriptEditor(self->mItemUUID, onLoad, onSave,
										 onSearchReplace, self);
	return self->mScriptEd;
}

LLLiveLSLEditor::LLLiveLSLEditor(const std::string& name, const LLRect& rect,
								 const std::string& title,
								 const LLUUID& obj_id, const LLUUID& item_id)
:	LLPreview(name, rect, title, item_id, obj_id, true, SCRIPT_MIN_WIDTH,
			  SCRIPT_MIN_HEIGHT),
	mObjectID(obj_id),
	mItemID(item_id),
	mScriptEd(NULL),
	mAskedForRunningInfo(false),
	mHaveRunningInfo(false),
	mCloseAfterSave(false),
	mIsModifiable(false),
	mIsSaving(false)
{
	bool is_new = false;
	if (mItemID.isNull())
	{
		mItemID.generate();
		is_new = true;
	}

	LLLiveLSLEditor::sInstances[mItemID ^ mObjectID] = this;

	LLCallbackMap::map_t factory_map;
	factory_map["script ed panel"] = LLCallbackMap(createScriptEdPanel, this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_live_lsleditor.xml",
												 &factory_map);

	mRunningCheckbox = getChild<LLCheckBoxCtrl>("running");
	mRunningCheckbox->setCommitCallback(onRunningCheckboxClicked);
	mRunningCheckbox->setCallbackUserData(this);
	mRunningCheckbox->setEnabled(false);

	childSetAction("Reset", onReset, this);
	childSetEnabled("Reset", true);

	mScriptEd->mEditor->makePristine();
	loadAsset(is_new);
	mScriptEd->mEditor->setFocus(true);

	if (!getHost())
	{
		LLRect cur_rect = getRect();
		translate(rect.mLeft - cur_rect.mLeft, rect.mTop - cur_rect.mTop);
	}

	setTitle(title);
	mScriptEd->setScriptName(title);

	mScriptRunningText = getString("script_running");
	mCannotRunText = getString("public_objects_can_not_run");
	mOutOfRange = getString("out_of_range");

	mExperiences = getChild<LLComboBox>("Experiences...");
	mExperiences->setCommitCallback(experienceChanged);
	mExperiences->setCallbackUserData(this);
	mExperiences->setVisible(false);

	mExperienceEnabled = getChild<LLCheckBoxCtrl>("enable_xp");
	mExperienceEnabled->set(false);
	mExperienceEnabled->setCommitCallback(onToggleExperience);
	mExperienceEnabled->setCallbackUserData(this);
	mExperienceEnabled->setEnabled(false);

	mViewProfileButton = getChild<LLButton>("view_profile");
	mViewProfileButton->setClickedCallback(onViewProfile, this);
	mViewProfileButton->setVisible(false);
}

LLLiveLSLEditor::~LLLiveLSLEditor()
{
	LLLiveLSLEditor::sInstances.erase(mItemID ^ mObjectID);
}

//virtual
void LLLiveLSLEditor::open()
{
	LLFloater::open();
}

//virtual
bool LLLiveLSLEditor::canClose()
{
	return mScriptEd->canClose();
}

//virtual
void LLLiveLSLEditor::draw()
{
	LLViewerObject* object = gObjectList.findObject(mObjectID);
	if (object && mAskedForRunningInfo && mHaveRunningInfo)
	{
		if (object->permAnyOwner())
		{
			mRunningCheckbox->setLabel(mScriptRunningText);
			mRunningCheckbox->setEnabled(!mIsSaving);

			if (object->permAnyOwner())
			{
				mRunningCheckbox->setLabel(mScriptRunningText);
				mRunningCheckbox->setEnabled(!mIsSaving);
			}
			else
			{
				mRunningCheckbox->setLabel(mCannotRunText);
				mRunningCheckbox->setEnabled(false);
				// *FIX: Set it to false so that the UI is correct for a box
				// that is released to public. It could be incorrect after a
				// release/claim cycle, but will be correct after clicking on
				// it.
				mRunningCheckbox->set(false);
				if (mScriptEd)
				{
					mScriptEd->getMonoCheckBox()->set(false);
				}
			}
		}
		else
		{
			mRunningCheckbox->setLabel(mCannotRunText);
			mRunningCheckbox->setEnabled(false);

			// *FIX: Set it to false so that the UI is correct for a box that
			// is released to public. It could be incorrect after a release/
			// claim cycle, but will be correct after clicking on it.
			mRunningCheckbox->set(false);
			if (mScriptEd)
			{
				mScriptEd->getMonoCheckBox()->setEnabled(false);
			}
			// object may have fallen out of range.
			mHaveRunningInfo = false;
		}
	}
	else if (!object)
	{
		setTitle(mOutOfRange);
		mRunningCheckbox->setEnabled(false);
		// Object may have fallen out of range.
		mHaveRunningInfo = false;
	}

	LLFloater::draw();
}

//virtual
void LLLiveLSLEditor::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLFloater::reshape(width, height, called_from_parent);
	if (!isMinimized())
	{
		// So that next time you open a script it will have the same height
		// and width (although not the same position).
		gSavedSettings.setRect("PreviewScriptRect", getRect());
	}
}

void LLLiveLSLEditor::closeIfNeeded()
{
	gWindowp->decBusyCount();
	if (mCloseAfterSave)
	{
		if (!mScriptEd->mAutosaveFilename.empty())
		{
			llinfos << "Remove autosave: " << mScriptEd->mAutosaveFilename
					<< llendl;
			LLFile::remove(mScriptEd->mAutosaveFilename);
		}
		close();
	}
}

void LLLiveLSLEditor::saveIfNeeded()
{
	LLViewerObject* object = gObjectList.findObject(mObjectID);
	if (!object)
	{
		gNotifications.add("SaveScriptFailObjectNotFound");
		return;
	}

	if (mItem.isNull() || !mItem->isFinished())
	{
		// NOTE: While the error message may not be exactly correct, it is
		// pretty close.
		gNotifications.add("SaveScriptFailObjectNotFound");
		return;
	}

	// Get the latest info about it. We used to be losing the script name on
	// save, because the viewer object version of the item, and the editor
	// version would get out of sync. Here's a good place to sync them back
	// up. *HACK: we "know" that mItemID refers to a LLInventoryItem...
	LLInventoryItem* inv_item =
		(LLInventoryItem*)object->getInventoryObject(mItemID);
	if (inv_item)
	{
		mItem->copyItem(inv_item);
	}

	// Do not need to save if we are pristine
	if (!mScriptEd->hasChanged())
	{
		return;
	}

	LLViewerRegion* regionp = object->getRegion();
	if (!regionp)
	{
		LLSD args;
		args["REASON"] = "cannot determine object region";
		gNotifications.add("SaveScriptFailReason", args);
		return;
	}
	const std::string& url = regionp->getCapability("UpdateScriptTask");
	if (url.empty())
	{
		LLSD args;
		args["REASON"] = "missing UpdateScriptTask capability";
		gNotifications.add("SaveScriptFailReason", args);
		return;
	}

	std::string buffer = mScriptEd->mSavedSources->getText();
	if (buffer.empty())
	{
		llwarns << "Empty or invalid script sources." << llendl;
		return;
	}

	// Save the script to asset server
	mScriptEd->mEditor->makePristine();
	mScriptEd->enableEdit(false);
	mScriptEd->enableSave(false);

	gWindowp->incBusyCount();
	mIsSaving = true;

	bool is_running = getChild<LLCheckBoxCtrl>("running")->get();
	LLBufferedAssetUploadInfo::task_uploaded_cb_t proc_ok =
		boost::bind(&LLLiveLSLEditor::finishLSLUpload, _1, _2, _3, _4,
					is_running);
	LLBufferedAssetUploadInfo::failed_cb_t proc_ko =
		boost::bind(&LLLiveLSLEditor::failedLSLUpload, _1, _2, _4);

	bool mono_checked = mScriptEd->monoChecked();
	LLScriptAssetUpload::TargetType_t type;
	if (!gIsInSecondLife || mono_checked)
	{
		type = LLScriptAssetUpload::MONO;
	}
	else
	{
		type = LLScriptAssetUpload::LSL2;
	}

	LLResourceUploadInfo::ptr_t
		info(new LLScriptAssetUpload(mObjectUUID, mItemUUID, type,
			 is_running, mScriptEd->getAssociatedExperience(), buffer, proc_ok,
			 proc_ko));
	LLViewerAssetUpload::enqueueInventoryUpload(url, info);
}

void LLLiveLSLEditor::callbackLSLCompileSucceeded(const LLUUID& task_id,
												  const LLUUID& item_id,
												  bool is_script_running)
{
	LL_DEBUGS("ScriptEditor") << "LSL Bytecode saved" << LL_ENDL;
	mScriptEd->addComment(getString("compile_success"));
	mScriptEd->addComment(getString("save_complete"));
	mScriptEd->enableEdit(true);
	mIsSaving = false;
	closeIfNeeded();
}

void LLLiveLSLEditor::callbackLSLCompileFailed(const LLSD& compile_errors)
{
	llwarns << "Compile failed !" << llendl;

	for (LLSD::array_const_iterator line = compile_errors.beginArray();
		 line < compile_errors.endArray(); ++line)
	{
		std::string error_message = line->asString();
		LLStringUtil::stripNonprintable(error_message);
		mScriptEd->addComment(error_message, true);
	}
	mScriptEd->selectFirstError();
	mScriptEd->enableEdit(true);
	mIsSaving = false;
	closeIfNeeded();
}

void LLLiveLSLEditor::loadAsset(bool is_new)
{
	if (is_new)
	{
		mScriptEd->setScriptText(HELLO_LSL, true);
		mScriptEd->enableSave(false);
		LLPermissions perm;
		perm.init(gAgentID, gAgentID, LLUUID::null, gAgent.getGroupID());
		perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE,
					   PERM_MOVE | PERM_TRANSFER);
		mItem = new LLViewerInventoryItem(mItemID, mObjectID, perm,
										  LLUUID::null,
										  LLAssetType::AT_LSL_TEXT,
										  LLInventoryType::IT_LSL,
										  DEFAULT_SCRIPT_NAME,
										  LLStringUtil::null,
										  LLSaleInfo::DEFAULT,
										  LLInventoryItem::II_FLAGS_NONE,
										  time_corrected());
		mAssetStatus = PREVIEW_ASSET_LOADED;
		requestExperiences();
		return;
	}

	LLViewerObject* object = gObjectList.findObject(mObjectID);
	if (!object)
	{
		llwarns << "Cannot find object " << mObjectID
				<< " in the viewer object list. Aborted." << llendl;
		return;
	}

	// HACK !  We "know" that mItemID refers to a LLViewerInventoryItem
	LLViewerInventoryItem* item =
		(LLViewerInventoryItem*)object->getInventoryObject(mItemID);
	if (item)
	{
		LLViewerRegion* regionp = object->getRegion();
		const std::string& url =
			regionp ? regionp->getCapability("GetMetadata")
					: gAgent.getRegionCapability("GetMetadata");
		LLExperienceCache* ecache = LLExperienceCache::getInstance();
		ecache->fetchAssociatedExperience(item->getParentUUID(),
										  item->getUUID(), url,
										  boost::bind(&LLLiveLSLEditor::setAssociatedExperience,
													  getDerivedHandle<LLLiveLSLEditor>(),
													  _1));

		bool god_like = gAgent.isGodlike();
		bool is_copyable = gAgent.allowOperation(PERM_COPY,
												 item->getPermissions(),
												 GP_OBJECT_MANIPULATE);
		mIsModifiable = gAgent.allowOperation(PERM_MODIFY,
											  item->getPermissions(),
											  GP_OBJECT_MANIPULATE);
		if (!god_like && (!is_copyable || !mIsModifiable))
		{
			mItem = new LLViewerInventoryItem();
			mScriptEd->setScriptText(LLStringUtil::null, false);
			mScriptEd->mEditor->makePristine();
			mScriptEd->mEditor->setEnabled(false);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else if (is_copyable || god_like)
		{
			mItem = new LLViewerInventoryItem(item);
			// Request the text from the object
			LLUUID* user_data = new LLUUID(mItemID ^ mObjectID);
			gAssetStoragep->getInvItemAsset(object->getRegion()->getHost(),
											gAgentID, gAgentSessionID,
											item->getPermissions().getOwner(),
											object->getID(),
											item->getUUID(),
											item->getAssetUUID(),
											item->getType(),
										   	onLoadComplete,
											(void*)user_data, true);
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_GetScriptRunning);
			msg->nextBlockFast(_PREHASH_Script);
			msg->addUUIDFast(_PREHASH_ObjectID, mObjectID);
			msg->addUUIDFast(_PREHASH_ItemID, mItemID);
			msg->sendReliable(object->getRegion()->getHost());
			mAskedForRunningInfo = true;
			mAssetStatus = PREVIEW_ASSET_LOADING;
		}
	}

	if (mItem.isNull())
	{
		mScriptEd->setScriptText(LLStringUtil::null, false);
		mScriptEd->mEditor->makePristine();
		mAssetStatus = PREVIEW_ASSET_LOADED;
		mIsModifiable = false;
	}

	requestExperiences();
}

void LLLiveLSLEditor::loadScriptText(const LLUUID& uuid,
									 LLAssetType::EType type)
{
	LLFileSystem file(uuid);
	S32 file_length = file.getSize();
	char* buffer = new char[file_length + 1];
	file.read((U8*)buffer, file_length);
	if (file.getLastBytesRead() != file_length || file_length <= 0)
	{
		llwarns << "Error reading " << uuid << ":" << type << llendl;
	}
	buffer[file_length] = '\0';
	mScriptEd->setScriptText(std::string(&buffer[0]), true);
	mScriptEd->mEditor->makePristine();
	delete[] buffer;

	const LLInventoryItem* item = getItem();
	if (item)
	{
		mScriptEd->setScriptName(item->getName());
	}
}

void LLLiveLSLEditor::setExperienceIds(const LLSD& experience_ids)
{
	mExperienceIds = experience_ids;
	updateExperienceControls();
}

void LLLiveLSLEditor::updateExperienceControls()
{
	if (mScriptEd->getAssociatedExperience().isNull())
	{
		mExperienceEnabled->set(false);
		mExperiences->setVisible(false);
		if (mExperienceIds.size() > 0)
		{
			mExperienceEnabled->setEnabled(true);
			mExperienceEnabled->setToolTip(getString("add_experiences"));
		}
		else
		{
			mExperienceEnabled->setEnabled(false);
			mExperienceEnabled->setToolTip(getString("no_experiences"));
		}
		mViewProfileButton->setVisible(false);
	}
	else
	{
		mExperienceEnabled->setToolTip(getString("experience_enabled"));
		mExperienceEnabled->setEnabled(getIsModifiable());
		mExperiences->setVisible(true);
		mExperienceEnabled->set(true);
		buildExperienceList();
	}
}

void LLLiveLSLEditor::buildExperienceList()
{
	mExperiences->clearRows();
	bool found = false;
	const LLUUID& associated = mScriptEd->getAssociatedExperience();
	LLUUID last;
	std::string name;
	LLScrollListItem* item;
	LLExperienceCache* expcache = LLExperienceCache::getInstance();
	for (LLSD::array_const_iterator it = mExperienceIds.beginArray(),
									end = mExperienceIds.endArray();
		 it != end; ++it)
	{
		LLUUID id = it->asUUID();
		EAddPosition position = ADD_BOTTOM;
		if (id == associated)
		{
			found = true;
			position = ADD_TOP;
		}

		const LLSD& experience = expcache->get(id);
		if (experience.isUndefined())
		{
			mExperiences->add(getString("loading"), id, position);
			last = id;
		}
		else
		{
			name = experience[LLExperienceCache::NAME].asString();
			if (name.empty())
			{
				name = LLTrans::getString("ExperienceNameUntitled");
			}
			mExperiences->add(name, id, position);
		}
	}

	if (!found)
	{
		const LLSD& experience = expcache->get(associated);
		if (experience.isDefined())
		{
			name = experience[LLExperienceCache::NAME].asString();
			if (name.empty())
			{
				name = LLTrans::getString("ExperienceNameUntitled");
			}
			item = mExperiences->add(name, associated, ADD_TOP);
		}
		else
		{
			item = mExperiences->add(getString("loading"), associated,
									 ADD_TOP);
			last = associated;
		}
		item->setEnabled(false);
	}

	if (last.notNull())
	{
		mExperiences->setEnabled(false);
		expcache->get(last,
					  boost::bind(&LLLiveLSLEditor::buildExperienceList,
								  this));
	}
	else
	{
		mExperiences->setEnabled(true);
		mExperiences->sortByName(true);
		mExperiences->setCurrentByIndex(mExperiences->getCurrentIndex());
		mViewProfileButton->setVisible(true);
	}
}

void LLLiveLSLEditor::requestExperiences()
{
	if (!getIsModifiable())
	{
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("GetCreatorExperiences");
	if (url.empty())
	{
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t succ =
		 boost::bind(&LLLiveLSLEditor::receiveExperienceIds, _1,
					 getDerivedHandle<LLLiveLSLEditor>());
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(url, succ);
}

//static
void LLLiveLSLEditor::receiveExperienceIds(LLSD result,
										   LLHandle<LLLiveLSLEditor> hparent)
{
	LLLiveLSLEditor* parent = hparent.get();
	if (parent)
	{
		parent->setExperienceIds(result["experience_ids"]);
	}
}

//static
void LLLiveLSLEditor::experienceChanged(LLUICtrl*, void* data)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)data;
	if (!self) return;

	LLScriptEditor* sed = self->mScriptEd;
	if (sed->getAssociatedExperience() !=
			self->mExperiences->getSelectedValue().asUUID())
	{
		sed->enableSave(self->getIsModifiable());
		sed->setAssociatedExperience(self->mExperiences->getSelectedValue().asUUID());
		self->updateExperienceControls();
	}
}

//static
void LLLiveLSLEditor::setAssociatedExperience(LLHandle<LLLiveLSLEditor> editor,
											  const LLSD& experience)
{
	LLLiveLSLEditor* self = editor.get();
	if (self)
	{
		LLUUID id;
		if (experience.has(LLExperienceCache::EXPERIENCE_ID))
		{
			id = experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
		}
		self->mScriptEd->setAssociatedExperience(id);
		self->updateExperienceControls();
	}
}

//static
void LLLiveLSLEditor::onToggleExperience(LLUICtrl*, void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (!self) return;

	LLUUID id;
	if (self->mExperienceEnabled->get() &&
		self->mScriptEd->getAssociatedExperience().isNull() &&
		self->mExperienceIds.size() > 0)
	{
		id = self->mExperienceIds.beginArray()->asUUID();
	}

	if (id != self->mScriptEd->getAssociatedExperience())
	{
		self->mScriptEd->enableSave(self->getIsModifiable());
	}
	self->mScriptEd->setAssociatedExperience(id);

	self->updateExperienceControls();
}

//static
void LLLiveLSLEditor::onRunningCheckboxClicked(LLUICtrl*, void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (!self) return;

	LLViewerObject* object = gObjectList.findObject(self->mObjectID);
	bool running = self->mRunningCheckbox->get();
//MK
	if (gRLenabled && !gRLInterface.canDetach(object))
	{
		self->mRunningCheckbox->set(!running);
		return;
	}
//mk
	if (object)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_SetScriptRunning);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_Script);
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemID);
		msg->addBoolFast(_PREHASH_Running, running);
		msg->sendReliable(object->getRegion()->getHost());
	}
	else
	{
		self->mRunningCheckbox->set(!running);
		gNotifications.add("CouldNotStartStopScript");
	}
}

//static
void LLLiveLSLEditor::onReset(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (!self) return;

	LLViewerObject* object = gObjectList.findObject(self->mObjectID);
//MK
	if (gRLenabled && !gRLInterface.canDetach(object))
	{
		return;
	}
//mk
	if (object)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ScriptReset);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_Script);
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemID);
		msg->sendReliable(object->getRegion()->getHost());
	}
	else
	{
		gNotifications.add("CouldNotStartStopScript");
	}
}

//static
void LLLiveLSLEditor::onLoad(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (self)
	{
		self->loadAsset();
	}
}

//static
void LLLiveLSLEditor::onLoadComplete(const LLUUID& asset_id,
									 LLAssetType::EType type, void* user_data,
									 S32 status, LLExtStat)
{
	LL_DEBUGS("ScriptEditor") << "Got asset UUID " << asset_id << LL_ENDL;

	LLUUID* xored_id = (LLUUID*)user_data;
	instances_map_t::iterator it = sInstances.find(*xored_id);
	delete xored_id;
	if (it == sInstances.end())
	{
		LL_DEBUGS("ScriptEditor") << "Stale callback, preview floater gone, aborted."
								  << LL_ENDL;
	}

	LLLiveLSLEditor* self = it->second;

	bool item_valid = self->getItem() != NULL;
	if (item_valid && status == LL_ERR_NOERR)
	{
		// All good
		self->loadScriptText(asset_id, type);
		self->mAssetStatus = PREVIEW_ASSET_LOADED;
		return;
	}

	gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);
	self->mAssetStatus = PREVIEW_ASSET_ERROR;

	if (!item_valid)
	{
		gNotifications.add("LoadScriptFailObjectNotFound");
	}
	else if (status == LL_ERR_FILE_EMPTY ||
			 status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE)
	{
		gNotifications.add("ScriptMissing");
	}
	else if (status == LL_ERR_INSUFFICIENT_PERMISSIONS)
	{
		gNotifications.add("ScriptNoPermissions");
	}
	else
	{
		gNotifications.add("UnableToLoadScript");
	}
}

//static
void LLLiveLSLEditor::onSave(void* userdata, bool close_after_save)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*) userdata;
	if (self)
	{
//MK
		if (gRLenabled)
		{
			LLViewerObject* object = gObjectList.findObject(self->mObjectID);
			if (!gRLInterface.canDetach(object))
			{
				return;
			}
		}
// mk
		self->mCloseAfterSave = close_after_save;
		self->saveIfNeeded();
	}
}

//static
void LLLiveLSLEditor::onSearchReplace(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (!self) return;

	LLScriptEditor* sed = self->mScriptEd;
	if (sed)
	{
		LLFloaterSearchReplace::show(sed->mEditor);
	}
}

//static
void LLLiveLSLEditor::finishLSLUpload(LLUUID item_id, LLUUID task_id,
									  LLUUID new_asset_id, LLSD response,
									  bool running)
{
	LLLiveLSLEditor* self = find(item_id, task_id);
	if (self)
	{
		self->mItem->setAssetUUID(new_asset_id);

		// Bytecode save completed
		if (response.has("compiled") && response["compiled"])
		{
			self->callbackLSLCompileSucceeded(task_id, item_id, running);
		}
		else
		{
			self->callbackLSLCompileFailed(response["errors"]);
		}
	}
}

//static
void LLLiveLSLEditor::failedLSLUpload(LLUUID item_id, LLUUID task_id,
									  std::string reason)
{
	LLLiveLSLEditor* self = find(item_id, task_id);
	if (self)
	{
		LLSD errors;
		errors.append(LLTrans::getString("AssetUploadFailed") + reason);
		self->callbackLSLCompileFailed(errors);
	}
}

//static
void LLLiveLSLEditor::onViewProfile(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (self && self->mExperienceEnabled->get())
	{
		LLUUID id = self->mScriptEd->getAssociatedExperience();
		if (id.notNull())
		{
			LLFloaterExperienceProfile::show(id);
		}
	}
}

//static
void LLLiveLSLEditor::processScriptRunningReply(LLMessageSystem* msg, void**)
{
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_Script, _PREHASH_ObjectID, object_id);
	LLUUID item_id;
	msg->getUUIDFast(_PREHASH_Script, _PREHASH_ItemID, item_id);
	LLUUID xored_id = item_id ^ object_id;
	instances_map_t::iterator it = sInstances.find(xored_id);
	if (it != sInstances.end())
	{
		LLLiveLSLEditor* self = it->second;
		self->mHaveRunningInfo = true;
		bool running;
		msg->getBoolFast(_PREHASH_Script, _PREHASH_Running, running);
		self->mRunningCheckbox->set(running);
		bool mono;
		msg->getBoolFast(_PREHASH_Script, "Mono", mono);
		LLCheckBoxCtrl* mono_check = self->mScriptEd->getMonoCheckBox();
		bool can_use_mono = gIsInSecondLife && self->getIsModifiable() &&
							have_script_upload_cap(object_id);
		mono_check->setEnabled(can_use_mono);
		mono_check->set(mono);
	}
}
