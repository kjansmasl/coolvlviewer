/**
 * @file llpreviewscript.h
 * @brief LLPreviewScript and LLLiveLSLEditor classes definition
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

#ifndef LL_LLPREVIEWSCRIPT_H
#define LL_LLPREVIEWSCRIPT_H

#include "llassettype.h"
#include "hbfileselector.h"

#include "llpreview.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLMessageSystem;
class LLScriptEditor;
class LLTextEditor;

// Used to view and edit a LSL script in your inventory.
class LLPreviewScript final : public LLPreview
{
protected:
	LOG_CLASS(LLPreviewScript);

public:
	LLPreviewScript(const std::string& name, const LLRect& rect,
					const std::string& title, const LLUUID& item_uuid);

	void open() override;

	static LLPreviewScript* getInstance(const LLUUID& uuid);
	LLTextEditor* getEditor();

	// Wrapper methods to setup LLScriptEditor implementation class defaults
	static void loadFunctions(const std::string& filename);
	static void refreshCachedSettings();

protected:
	bool canClose() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	LL_INLINE const char* getTitleName() const override	{ return "Script"; }

	void loadAsset() override;

	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	static void onSave(void* userdata, bool close_after_save);

private:
	void saveIfNeeded();
	void closeIfNeeded();

	void callbackLSLCompileSucceeded();
	void callbackLSLCompileFailed(const LLSD& compile_errors);

	static void finishLSLUpload(LLUUID item_id, LLSD response);
	static void failedLSLUpload(LLUUID item_id, std::string reason);

	static void onLoadComplete(const LLUUID& asset_id, LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat);

	static void* createScriptEdPanel(void* userdata);

protected:
	LLScriptEditor* mScriptEd;
};

// Used to view and edit a LSL script that is attached to an object.
class LLLiveLSLEditor final : public LLPreview
{
protected:
	LOG_CLASS(LLLiveLSLEditor);

public:
	LLLiveLSLEditor(const std::string& name, const LLRect& rect,
					const std::string& title,
					const LLUUID& object_id, const LLUUID& item_id);
	~LLLiveLSLEditor() override;


	static LLLiveLSLEditor* show(const LLUUID& item_id, const LLUUID& object_id);
	static void hide(const LLUUID& item_id, const LLUUID& object_id);
	static LLLiveLSLEditor* find(const LLUUID& item_id, const LLUUID& object_id);

	// Overide LLPreview::open() to avoid calling loadAsset twice.
	void open() override;

	void setExperienceIds(const LLSD& experience_ids);

	// Callback for message system, linked in llstartup.cpp
	static void processScriptRunningReply(LLMessageSystem* msg, void**);

protected:
	bool canClose() override;
	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	// This is called via LLPreview::loadAsset() virtual method
	LL_INLINE void loadAsset() override					{ loadAsset(false); }

	// Evaluated on assert loading
	LL_INLINE bool getIsModifiable() const				{ return mIsModifiable; }

	static void finishLSLUpload(LLUUID item_id, LLUUID task_id,
								LLUUID new_asset_id, LLSD response,
								bool running);
	static void failedLSLUpload(LLUUID item_id, LLUUID task_id,
								std::string reason);
	static void receiveExperienceIds(LLSD result,
									 LLHandle<LLLiveLSLEditor> parent);

	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	static void onSave(void* userdata, bool close_after_save);

private:
	void buildExperienceList();
	void updateExperienceControls();
	void requestExperiences();

	void saveIfNeeded();
	void closeIfNeeded();

	void loadAsset(bool is_new);

	void callbackLSLCompileSucceeded(const LLUUID& task_id,
									 const LLUUID& item_id,
									 bool is_script_running);
	void callbackLSLCompileFailed(const LLSD& compile_errors);

	static void onLoadComplete(const LLUUID& asset_id, LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat);
	static void onRunningCheckboxClicked(LLUICtrl*, void* userdata);
	static void onReset(void* userdata);

	static void experienceChanged(LLUICtrl*, void* data);
	static void setAssociatedExperience(LLHandle<LLLiveLSLEditor> editor,
										const LLSD& experience);
	static void onToggleExperience(LLUICtrl*, void* userdata);

	static void onViewProfile(void* userdata);

	void loadScriptText(const LLUUID& id, LLAssetType::EType type);

	static void onErrorList(LLUICtrl*, void* user_data);

	static void* createScriptEdPanel(void* userdata);

protected:
	typedef fast_hmap<LLUUID, LLLiveLSLEditor*> instances_map_t;
	static instances_map_t				sInstances;

	LLPointer<LLViewerInventoryItem>	mItem;

	// The inventory item this script is associated with:
	LLUUID								mItemID;
	// The object this script is associated with:
	LLUUID								mObjectID;

	LLScriptEditor*						mScriptEd;

	LLButton*							mResetButton;
	LLButton*							mViewProfileButton;
	LLCheckBoxCtrl*						mRunningCheckbox;
	LLCheckBoxCtrl*						mExperienceEnabled;
	LLComboBox*							mExperiences;

	LLSD								mExperienceIds;

	std::string							mScriptRunningText;
	std::string							mCannotRunText;
	std::string							mOutOfRange;

	bool								mIsNew;
	bool								mAskedForRunningInfo;
	bool								mHaveRunningInfo;
	bool								mCloseAfterSave;

private:
	bool								mIsModifiable;
	bool								mIsSaving;
};

#endif  // LL_LLPREVIEWSCRIPT_H
