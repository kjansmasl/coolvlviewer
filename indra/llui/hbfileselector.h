/**
 * @file hbfileselector.h
 * @brief The HBFileSelector class declaration
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Henri Beauchamp
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

#ifndef LL_HBFILESELECTOR_H
#define LL_HBFILESELECTOR_H

#include <deque>
#include <vector>

#include "llerror.h"
#include "hbfastmap.h"
#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLFlyoutButton;
class LLLineEditor;
class LLScrollListCtrl;
class LLTextBox;

class HBFileSelector final : public LLFloater
{
protected:
	LOG_CLASS(HBFileSelector);

public:
	~HBFileSelector() override;

	bool postBuild() override;
	void draw() override;

	enum ELoadFilter
	{
		FFLOAD_ALL		= 1,
		FFLOAD_TEXT		= 2,
		FFLOAD_XML		= 3,
		FFLOAD_XUI		= 4,
		FFLOAD_SCRIPT	= 5,
		FFLOAD_SOUND	= 6,
		FFLOAD_ANIM		= 7,
		FFLOAD_MODEL	= 8,
		FFLOAD_OBJ		= 9,	// Not used (no loading of *.obj files)
		FFLOAD_TERRAIN	= 10,
		FFLOAD_IMAGE	= 11,
		FFLOAD_LUA		= 12,
		FFLOAD_GLTF		= 13,
		FFLOAD_NONE		= 255,
	};

	enum ESaveFilter
	{
		FFSAVE_ALL		= 1,
		FFSAVE_TXT		= 2,
		FFSAVE_XML		= 3,
		FFSAVE_XUI		= 4,
		FFSAVE_LSL		= 5,
		FFSAVE_WAV		= 6,
		FFSAVE_BVH		= 7,
		FFSAVE_DAE		= 8,
		FFSAVE_OBJ		= 9,
		FFSAVE_RAW		= 10,
		FFSAVE_TGA		= 11,
		FFSAVE_PNG		= 12,
		FFSAVE_JPG		= 13,
		FFSAVE_J2C		= 14,
		FFSAVE_BMP		= 15,
		FFSAVE_GLTF		= 16,
		FFSAVE_NONE		= 255,
	};

	typedef void (*HBLoadFileCallback)(ELoadFilter type, std::string& filename,
									   void* user_data);

	typedef void (*HBLoadFilesCallback)(ELoadFilter type,
										std::deque<std::string>& files,
										void* user_data);

	typedef void (*HBSaveFileCallback)(ESaveFilter type, std::string& filename,
									   void* user_data);

	typedef void (*HBDirPickCallback)(std::string& dirname, void* user_data);

	static void loadFile(ELoadFilter filter, HBLoadFileCallback callback,
						 void* user_data = NULL);

	static void loadFiles(ELoadFilter filter, HBLoadFilesCallback callback,
						  void* user_data = NULL);

	static void saveFile(ESaveFilter filter, std::string suggestion,
						 HBSaveFileCallback callback, void* user_data = NULL);

	static void pickDirectory(std::string suggestion,
							  HBDirPickCallback callback,
							  void* user_data = NULL);

	static bool isInUse()				{ return sInstance != NULL; }

	static void saveDefaultPaths(const std::string& filename);
	static void loadDefaultPaths(const std::string& filename);

private:
	enum EContext
	{
		CONTEXT_UNKNOWN		= 0,
		CONTEXT_DEFAULT		= 1,
		CONTEXT_TXT			= 2,
		CONTEXT_XML			= 3,
		CONTEXT_XUI			= 4,
		CONTEXT_LSL			= 5,
		CONTEXT_SOUND		= 6,
		CONTEXT_ANIM		= 7,
		CONTEXT_MODEL		= 8,
		CONTEXT_OBJ			= 9,	// Not used (using CONTEXT_MODEL for *.obj)
		CONTEXT_RAW			= 10,
		CONTEXT_IMAGE		= 11,
		CONTEXT_LUA			= 12,
		CONTEXT_MATERIAL	= 13,
		CONTEXT_END
	};

	HBFileSelector(ELoadFilter filter, HBLoadFileCallback callback,
				   void* user_data);

	HBFileSelector(ELoadFilter filter, HBLoadFilesCallback callback,
				   void* user_data);

	HBFileSelector(ESaveFilter filter, std::string& suggestion,
				   HBSaveFileCallback callback, void* user_data);

	HBFileSelector(std::string& suggestion,
				   HBDirPickCallback callback, void* user_data);

	void init(void* user_data);
	void setValidExtensions();
	void setPrompt();
	void setPathFromContext();
	bool isCurrentPathAtRoot();
	bool isFileExtensionValid(const std::string& filename);
	void setSelectionData();
	void doCallback();

	static void onButtonCreate(void* user_data);
	static void onButtonRefresh(void* user_data);
	static void onButtonCancel(void* user_data);
	static void onButtonOK(void* user_data);
	static void onButtonDirLevel(LLUICtrl* ctrl, void* user_data);
	static void onSelectDirectory(LLUICtrl*, void* user_data);
	static void onLevelDown(void* user_data);
	static void onSelectFile(LLUICtrl*, void* user_data);
	static void onCommitCheckBox(LLUICtrl*, void* user_data);
	static bool onHandleKeyCallback(KEY key, MASK mask, LLLineEditor* caller,
									void* user_data);
	static void onKeystrokeCallback(LLLineEditor* caller, void* user_data);

private:
	void					(*mLoadFileCallback)(ELoadFilter type,
												 std::string& filename,
												 void* user_data);
	void					(*mLoadFilesCallback)(ELoadFilter type,
												  std::deque<std::string>& files,
												  void* user_data);
	void					(*mSaveFileCallback)(ESaveFilter type,
												 std::string& filename,
												 void* user_data);
	void					(*mDirPickCallback)(std::string& dirname,
												void* user_data);

	void*					mCallbackUserData;

	LLFlyoutButton*			mDirLevelFlyoutBtn;
	LLButton*				mCreateBtn;
	LLButton*				mRefreshBtn;
	LLButton*				mCancelBtn;
	LLButton*				mOKBtn;
	LLCheckBoxCtrl*			mShowHiddenCheck;
	LLCheckBoxCtrl*			mShowAllTypesCheck;
	LLLineEditor*			mInputLine;
	LLScrollListCtrl*		mDirectoriesList;
	LLScrollListCtrl*		mFilesList;
	LLTextBox*				mPromptTextBox;
	LLTextBox*				mPathTextBox;

	ELoadFilter				mLoadFilter;
	ESaveFilter				mSaveFilter;
	EContext				mContext;

	std::string				mCurrentSelection;
	std::string				mCurrentEntry;
	std::deque<std::string>	mFiles;
	std::vector<std::string> mValidExtensions;
	std::string				mFileTypeDescription;
	std::string				mCurrentPath;

	bool					mIsDirty;
	bool					mCallbackDone;
	bool					mMultiple;
	bool					mSavePicker;
	bool					mDirPicker;
	bool					mCreatingDirectory;

	static HBFileSelector*	sInstance;

	typedef fast_hmap<S32, std::string> context_map_t;
	static context_map_t	sContextToPathMap;

	static std::string		sLastPath;
};

#endif	// LL_HBFILESELECTOR_H
