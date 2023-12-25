/**
 * @file hbfileselector.cpp
 * @brief The HBFileSelector class definition
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

#include "linden_common.h"

#include "hbfileselector.h"

#include "llapp.h"				// For isExiting()
#include "llcallbacklist.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldate.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

// Position in a path string where the root delimiter is found, i.e. position
// of "\" in (e.g.) "C:\" for Windows and position of "/" (0) for other OSes
#if LL_WINDOWS
# define ROOT_DELIMITER_POS 2
#else
# define ROOT_DELIMITER_POS 0
#endif

// Static variables
HBFileSelector* HBFileSelector::sInstance = NULL;
HBFileSelector::context_map_t HBFileSelector::sContextToPathMap;
std::string HBFileSelector::sLastPath;

HBFileSelector::HBFileSelector(ELoadFilter filter,
							   HBLoadFileCallback callback, void* user_data)
:	mLoadFilter(filter),
	mSaveFilter(FFSAVE_NONE),
	mMultiple(false),
	mSavePicker(false),
	mDirPicker(false),
	mLoadFileCallback(callback),
	mLoadFilesCallback(NULL),
	mSaveFileCallback(NULL),
	mDirPickCallback(NULL)
{
	init(user_data);
}

HBFileSelector::HBFileSelector(ELoadFilter filter,
							   HBLoadFilesCallback callback, void* user_data)
:	mLoadFilter(filter),
	mSaveFilter(FFSAVE_NONE),
	mMultiple(true),
	mSavePicker(false),
	mDirPicker(false),
	mLoadFileCallback(NULL),
	mLoadFilesCallback(callback),
	mSaveFileCallback(NULL),
	mDirPickCallback(NULL)
{
	init(user_data);
}

HBFileSelector::HBFileSelector(ESaveFilter filter, std::string& suggestion,
							   HBSaveFileCallback callback, void* user_data)
:	mLoadFilter(FFLOAD_NONE),
	mSaveFilter(filter),
	mMultiple(false),
	mSavePicker(true),
	mDirPicker(false),
	mLoadFileCallback(NULL),
	mLoadFilesCallback(NULL),
	mSaveFileCallback(callback),
	mDirPickCallback(NULL),
	mCurrentEntry(suggestion)
{
	init(user_data);
}

HBFileSelector::HBFileSelector(std::string& suggestion,
							   HBDirPickCallback callback, void* user_data)
:	mLoadFilter(FFLOAD_NONE),
	mSaveFilter(FFSAVE_NONE),
	mMultiple(false),
	mSavePicker(false),
	mDirPicker(true),
	mLoadFileCallback(NULL),
	mLoadFilesCallback(NULL),
	mSaveFileCallback(NULL),
	mDirPickCallback(callback),
	mCurrentEntry(suggestion)
{
	init(user_data);
}

void HBFileSelector::init(void* user_data)
{
	sInstance = this;

	mCallbackUserData = user_data;
	mCallbackDone = mIsDirty = mCreatingDirectory = false;
	mContext = CONTEXT_UNKNOWN;

	std::string xui_file = mDirPicker ? "floater_dirselector.xml"
									  : "floater_fileselector.xml";
    LLUICtrlFactory::getInstance()->buildFloater(this, xui_file);
}

HBFileSelector::~HBFileSelector()
{
	if (!mCallbackDone && !LLApp::isExiting())
	{
		mCurrentSelection.clear();
		mFiles.clear();
		doCallback();
	}

	sInstance = NULL;
}

bool HBFileSelector::postBuild()
{
	mDirectoriesList = getChild<LLScrollListCtrl>("directories");
	if (mDirPicker)
	{
		mDirectoriesList->setCommitOnSelectionChange(true);
		mDirectoriesList->setCommitCallback(onSelectDirectory);
	}
	mDirectoriesList->setDoubleClickCallback(onLevelDown);
	mDirectoriesList->setCallbackUserData(this);

	if (mDirPicker)
	{
		mFilesList = NULL;
		mShowAllTypesCheck = NULL;
	}
	else
	{
		mFilesList = getChild<LLScrollListCtrl>("files");
		mFilesList->setAllowMultipleSelection(mMultiple);
		mFilesList->setCommitOnSelectionChange(true);
		mFilesList->setCommitCallback(onSelectFile);
		mFilesList->setDoubleClickCallback(onButtonOK);
		mFilesList->setCallbackUserData(this);

		mShowAllTypesCheck = getChild<LLCheckBoxCtrl>("all_files");
		mShowAllTypesCheck->setCommitCallback(onCommitCheckBox);
		mShowAllTypesCheck->setCallbackUserData(this);

		setValidExtensions();
	}

	mPromptTextBox = getChild<LLTextBox>("prompt");
	mPathTextBox = getChild<LLTextBox>("path");

	mInputLine = getChild<LLLineEditor>("selection");
	mInputLine->setOnHandleKeyCallback(onHandleKeyCallback, this);
	mInputLine->setKeystrokeCallback(onKeystrokeCallback);
	mInputLine->setCallbackUserData(this);
	mInputLine->setEnabled(mSavePicker);

	mShowHiddenCheck = getChild<LLCheckBoxCtrl>("show_hidden");
	mShowHiddenCheck->setCommitCallback(onCommitCheckBox);
	mShowHiddenCheck->setCallbackUserData(this);

	mDirLevelFlyoutBtn = getChild<LLFlyoutButton>("dir_level");
	mDirLevelFlyoutBtn->setCommitCallback(onButtonDirLevel);
	mDirLevelFlyoutBtn->setCallbackUserData(this);

	mCreateBtn = getChild<LLButton>("create");
	mCreateBtn->setClickedCallback(onButtonCreate, this);
	mCreateBtn->setEnabled(mSavePicker || mDirPicker);

	mRefreshBtn = getChild<LLButton>("refresh");
	mRefreshBtn->setClickedCallback(onButtonRefresh, this);

	mCancelBtn = getChild<LLButton>("cancel");
	mCancelBtn->setClickedCallback(onButtonCancel, this);

	mOKBtn = getChild<LLButton>("ok");
	mOKBtn->setClickedCallback(onButtonOK, this);

	setPathFromContext();

	setPrompt();

	mIsDirty = true;

	return true;
}

void HBFileSelector::draw()
{
	if (mIsDirty)
	{
		mPathTextBox->setText(mCurrentPath);
		mDirectoriesList->deleteAllItems();
		if (mFilesList)
		{
			mFilesList->deleteAllItems();
		}
		std::string timeformat;
		if (LLUI::sConfigGroup)
		{
			timeformat = LLUI::sConfigGroup->getString("ShortDateFormat") +
						 " " +
						 LLUI::sConfigGroup->getString("LongTimeFormat");
		}
#if !LL_WINDOWS
		std::string path = mCurrentPath + LL_DIR_DELIM_STR;
#else
		std::string path;
		if (!mCurrentPath.empty())
		{
			path = mCurrentPath + LL_DIR_DELIM_STR;
		}
#endif
		std::string filename;
		LLSD element;
		LLUUID id, selected_id;
		bool selection_is_dir = false;
#if !LL_WINDOWS
		if (!mCurrentEntry.empty() && mCurrentEntry[0] == '.')
		{
			mShowHiddenCheck->set(true);
		}
#endif
		bool with_hidden = mShowHiddenCheck->get();
		LLDirIterator iter(path, NULL, DI_ALL);
		while (iter.next(filename))
		{
			if (!with_hidden && iter.isHidden())
			{
				continue;	// Do not list hidden entries
			}

			bool is_dir = iter.isDirectory();

			element.clear();
			id.generate();
			if (filename == mCurrentEntry)
			{
				selected_id = id;
				selection_is_dir = is_dir;
			}
			element["id"] = id;
			if (is_dir)
			{
				element["columns"][0]["column"] = "dirname_col";
				element["columns"][0]["value"] = filename;
				if (iter.isLink())
				{
					element["columns"][0]["font-style"] = "ITALIC";
				}
				mDirectoriesList->addElement(element, ADD_SORTED);
			}
			else if (mFilesList && isFileExtensionValid(filename))
			{
				element["columns"][0]["column"] = "name_col";
				element["columns"][0]["value"] = filename;
				if (iter.isLink())
				{
					element["columns"][0]["font-style"] = "ITALIC";
				}
				element["columns"][1]["column"] = "size_col";
				element["columns"][1]["value"] = (LLSD::Integer)iter.getSize();
				element["columns"][2]["column"] = "date_col";
				element["columns"][2]["type"] = "date";
				element["columns"][2]["format"] = timeformat;
				element["columns"][2]["value"] = LLDate(iter.getTimeStamp());
				mFilesList->addElement(element, ADD_SORTED);
			}
		}

		// Set the current selection, if any:
		if (mCurrentEntry.empty())
		{
			mInputLine->clear();
		}
		else
		{
			mInputLine->setText(mCurrentEntry);
			mInputLine->setCursorToEnd();
			bool got_it = mSavePicker;
			if (selected_id.notNull())
			{
				if (selection_is_dir)
				{
					if (mDirPicker &&
						mDirectoriesList->selectByID(selected_id))
					{
						mDirectoriesList->scrollToShowSelected();
						got_it = true;
					}
				}
				else if (mFilesList && mFilesList->selectByID(selected_id))
				{
					mFilesList->scrollToShowSelected();
					got_it = true;
				}
			}
			if (!got_it)
			{
				mCurrentEntry.clear();
			}
		}

		mIsDirty = false;
	}

	LLFloater::draw();
}

void HBFileSelector::setValidExtensions()
{
	mContext = CONTEXT_DEFAULT;
	mValidExtensions.clear();
	if (mSavePicker)
	{
		mFileTypeDescription = getString("any_file") + " (*.*)";
	}
	else if (!mDirPicker)
	{
		mFileTypeDescription = getString("all_files") + " (*.*)";
	}

	if (!mShowAllTypesCheck || mShowAllTypesCheck->get())
	{
		return;
	}

	if (mSavePicker && mSaveFilter != FFSAVE_NONE && mSaveFilter != FFSAVE_ALL)
	{
		switch (mSaveFilter)
		{
			case FFSAVE_TXT:
				mValidExtensions.emplace_back("txt");
				mFileTypeDescription = getString("txt_file") + " (*.txt)";
				mContext = CONTEXT_TXT;
				break;

			case FFSAVE_XML:
				mValidExtensions.emplace_back("xml");
				mFileTypeDescription = getString("xml_file") + " (*.xml)";
				mContext = CONTEXT_XML;
				break;

			case FFSAVE_XUI:
				mValidExtensions.emplace_back("xml");
				mFileTypeDescription = getString("xui_file") + " (*.xml)";
				mContext = CONTEXT_XUI;
				break;

			case FFSAVE_LSL:
				mValidExtensions.emplace_back("lsl");
				mFileTypeDescription = getString("lsl_file") + " (*.lsl)";
				mContext = CONTEXT_LSL;
				break;

			case FFSAVE_WAV:
				mValidExtensions.emplace_back("wav");
				mFileTypeDescription = getString("wav_file") + " (*.wav)";
				mContext = CONTEXT_SOUND;
				break;

			case FFSAVE_BVH:
				mValidExtensions.emplace_back("bvh");
				mFileTypeDescription = getString("bvh_file") + " (*.bvh)";
				mContext = CONTEXT_ANIM;
				break;

			case FFSAVE_DAE:
				mValidExtensions.emplace_back("dae");
				mFileTypeDescription = getString("dae_file") + " (*.dae)";
				mContext = CONTEXT_MODEL;
				break;

			case FFSAVE_OBJ:
				mValidExtensions.emplace_back("obj");
				mFileTypeDescription = getString("obj_file") + " (*.obj)";
				mContext = CONTEXT_MODEL;
				break;

			case FFSAVE_RAW:
				mValidExtensions.emplace_back("raw");
				mFileTypeDescription = getString("raw_file") + " (*.raw)";
				mContext = CONTEXT_RAW;
				break;

			case FFSAVE_TGA:
				mValidExtensions.emplace_back("tga");
				mFileTypeDescription = getString("tga_file") + " (*.tga)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFSAVE_PNG:
				mValidExtensions.emplace_back("png");
				mFileTypeDescription = getString("png_file") + " (*.png)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFSAVE_JPG:
				mValidExtensions.emplace_back("jpg");
				mFileTypeDescription = getString("jpg_file") +
									   " (*.jpg;*.jpeg)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFSAVE_J2C:
				mValidExtensions.emplace_back("j2c");
				mFileTypeDescription = getString("j2c_file") + " (*.j2c)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFSAVE_BMP:
				mValidExtensions.emplace_back("bmp");
				mFileTypeDescription = getString("bmp_file") + " (*.bmp)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFSAVE_GLTF:
				mValidExtensions.emplace_back("glb");
				mFileTypeDescription = getString("gltf_file") + " (*.glb)";
				mContext = CONTEXT_MATERIAL;
				break;

			default:
				break;
		}
	}
	else if (mLoadFilter != FFLOAD_NONE && mLoadFilter != FFLOAD_ALL)
	{
		switch (mLoadFilter)
		{
			case FFLOAD_TEXT:
				mValidExtensions.emplace_back("txt");
				mFileTypeDescription = getString("text_files") + " (*.txt)";
				mContext = CONTEXT_TXT;
				break;

			case FFLOAD_XML:
				mValidExtensions.emplace_back("xml");
				mFileTypeDescription = getString("xml_files") + " (*.xml)";
				mContext = CONTEXT_XML;
				break;

			case FFLOAD_XUI:
				mValidExtensions.emplace_back("xml");
				mFileTypeDescription = getString("xui_files") + " (*.xml)";
				mContext = CONTEXT_XUI;
				break;

			case FFLOAD_SCRIPT:
				mValidExtensions.emplace_back("lsl");
				mFileTypeDescription = getString("script_files") + " (*.lsl)";
				mContext = CONTEXT_LSL;
				break;

			case FFLOAD_SOUND:
				mValidExtensions.emplace_back("wav");
				mValidExtensions.emplace_back("dsf");
				mFileTypeDescription = getString("sound_files") +
									   " (*.wav;*.dsf)";
				mContext = CONTEXT_SOUND;
				break;

			case FFLOAD_ANIM:
				mValidExtensions.emplace_back("bvh");
				mValidExtensions.emplace_back("anim");
				mFileTypeDescription = getString("animation_files") +
									   " (*.bvh;*.anim)";
				mContext = CONTEXT_ANIM;
				break;

			case FFLOAD_MODEL:
				mValidExtensions.emplace_back("dae");
				mValidExtensions.emplace_back("glb");
				mValidExtensions.emplace_back("gltf");
				mFileTypeDescription = getString("model_files") +
									   " (*.dae;*.glb;*.gltf)";
				mContext = CONTEXT_MODEL;
				break;

			case FFLOAD_TERRAIN:
				mValidExtensions.emplace_back("raw");
				mFileTypeDescription = getString("raw_files") + " (*.raw)";
				mContext = CONTEXT_RAW;
				break;

			case FFLOAD_IMAGE:
				mValidExtensions.emplace_back("tga");
				mValidExtensions.emplace_back("png");
				mValidExtensions.emplace_back("jpg");
				mValidExtensions.emplace_back("jpeg");
				mValidExtensions.emplace_back("bmp");
				mFileTypeDescription = getString("image_files") +
									   " (*.tga;*.png;*.jpg;*.jpeg;*.bmp)";
				mContext = CONTEXT_IMAGE;
				break;

			case FFLOAD_LUA:
				mValidExtensions.emplace_back("lua");
				mValidExtensions.emplace_back("luac");
				mFileTypeDescription = getString("lua_files") +
									   " (*.lua;*.luac)";
				mContext = CONTEXT_LUA;
				break;

			case FFLOAD_GLTF:
				mValidExtensions.emplace_back("glb");
				mValidExtensions.emplace_back("gltf");
				mFileTypeDescription = getString("gltf_files") +
									   " (*.glb;*.gltf)";
				mContext = CONTEXT_MATERIAL;
				break;

			default:
				break;
		}
	}
}

void HBFileSelector::setPrompt()
{
	std::string prompt;
	bool got_file_info = !mFileTypeDescription.empty();
	if (mCreatingDirectory)
	{
		prompt = getString("new_directory");
	}
	else if (mSavePicker && got_file_info)
	{
		prompt = getString("prompt_save") + " " + mFileTypeDescription;
	}
	else if (mMultiple && got_file_info)
	{
		prompt = getString("prompt_load_multiple") + " " +
				 mFileTypeDescription;
	}
	else if (!mMultiple && !mSavePicker && got_file_info)
	{
		prompt = getString("prompt_load_one") + " " + mFileTypeDescription;
	}
	else
	{
		prompt = getString("default_prompt");
	}
	mPromptTextBox->setText(prompt);
	if (mSaveFilter == FFSAVE_NONE)
	{
		mPromptTextBox->setColor(LLColor4::green);
	}
	else
	{
		mPromptTextBox->setColor(LLColor4::yellow);
	}
}

bool HBFileSelector::isFileExtensionValid(const std::string& filename)
{
	if (mValidExtensions.empty() ||
		(mShowAllTypesCheck && mShowAllTypesCheck->get()))
	{
		return true;
	}

	std::string tmp = filename;
	LLStringUtil::toLower(tmp);
	size_t j = tmp.rfind('.');
	if (j == std::string::npos || j == tmp.length() - 1)
	{
		return false;
	}
	tmp = tmp.substr(j + 1);
	for (U32 i = 0, count = mValidExtensions.size(); i < count; ++i)
	{
		if (mValidExtensions[i] == tmp)
		{
			return true;
		}
	}

	return false;
}

void HBFileSelector::setPathFromContext()
{
	if (mDirPicker)
	{
		std::string tmp = mCurrentEntry;
		if (!tmp.empty())
		{
			// Remove trailing delimiter(s)
			while (tmp.length() &&
				   tmp.rfind(LL_DIR_DELIM_CHR) == tmp.length() - 1)
			{
				tmp = tmp.substr(0, tmp.length() - 1);
			}

			size_t i = tmp.rfind(LL_DIR_DELIM_CHR);
			if (i != std::string::npos && i >= 1)
			{
				// Suggested directory selection
				mCurrentEntry = tmp.substr(i + 1);
				// Parent directory path
				mCurrentPath = tmp.substr(0, i);
			}
			else
			{
				// Suggested directory selection = root directory
				mCurrentEntry = tmp;
				mCurrentPath.clear();
			}
		}
		else
		{
			mCurrentPath = sLastPath;
		}
	}
	else
	{
		context_map_t::iterator it = sContextToPathMap.find(mContext);
		if (it != sContextToPathMap.end())
		{
			mCurrentPath = it->second;
		}
		else
		{
			mCurrentPath.clear();
		}
		// If the saved path is not valid any more, try to find the deepest
		// directory that used to contain it...
		while (!mCurrentPath.empty() && !LLFile::exists(mCurrentPath))
		{
			size_t i = mCurrentPath.rfind(LL_DIR_DELIM_CHR);
			if (i == std::string::npos || i == ROOT_DELIMITER_POS)
			{
				mCurrentPath.clear();
				break;
			}
			mCurrentPath.erase(i);
		}
		if (mCurrentPath.empty())
		{
			if (mContext == CONTEXT_XUI)
			{
				#define XUI_DIR LL_DIR_DELIM_STR "xui" LL_DIR_DELIM_STR "en-us"
				mCurrentPath = gDirUtilp->getSkinDir() + XUI_DIR;
			}
			else
			{
				mCurrentPath = sLastPath;
			}
		}
	}
	if (mCurrentPath.empty() || !LLFile::exists(mCurrentPath))
	{
		if (!sLastPath.empty() && LLFile::exists(sLastPath))
		{
			mCurrentPath = sLastPath;
		}
		else
		{
			mCurrentPath = gDirUtilp->getOSUserDir();
		}
	}
	isCurrentPathAtRoot();
}

bool HBFileSelector::isCurrentPathAtRoot()
{
	// Remove trailing delimiter(s)
	mCurrentPath.erase(mCurrentPath.find_last_not_of(LL_DIR_DELIM_CHR) + 1);

	return mCurrentPath.empty();
}

void HBFileSelector::setSelectionData()
{
	mFiles.clear();
#if !LL_WINDOWS
	std::string path = mCurrentPath + LL_DIR_DELIM_STR;
#else
	std::string path;
	if (!mCurrentPath.empty())
	{
		path = mCurrentPath + LL_DIR_DELIM_STR;
	}
#endif
	std::string filename;
	LLScrollListItem* item = NULL;
	if (mDirPicker)
	{
		item = mDirectoriesList->getFirstSelected();
	}
	else if (mFilesList)
	{
		if (mMultiple)
		{
			std::vector<LLScrollListItem*> items;
			items = mFilesList->getAllSelected();
			if (!items.empty())
			{
				for (std::vector<LLScrollListItem*>::const_iterator
						iter = items.begin(), end = items.end();
					 iter != end; ++iter)
				{
					item = *iter;
					if (item && item->getColumn(0))
					{
						filename = item->getColumn(0)->getValue().asString();
						mFiles.emplace_back(path + filename);
					}
				}
			}
		}
		item = mFilesList->getFirstSelected();
	}

	if (item && item->getColumn(0))
	{
		filename = item->getColumn(0)->getValue().asString();
		mCurrentSelection = path + filename;
		mInputLine->setText(filename);
	}
	else if ((mDirPicker || mSavePicker) && !mInputLine->getText().empty())
	{
		mCurrentSelection = path + mInputLine->getText();
	}
}

void HBFileSelector::doCallback()
{
	if (!mCallbackDone)
	{
		mCallbackDone = true;
		if (mLoadFileCallback)
		{
			mLoadFileCallback(mLoadFilter, mCurrentSelection,
							  mCallbackUserData);
		}
		else if (mLoadFilesCallback)
		{
			mLoadFilesCallback(mLoadFilter, mFiles, mCallbackUserData);
		}
		else if (mSaveFileCallback)
		{
			if (!mCurrentSelection.empty() &&
				!isFileExtensionValid(mCurrentSelection))
			{
				mCurrentSelection += "." + mValidExtensions[0];
			}
			mSaveFileCallback(mSaveFilter, mCurrentSelection,
							  mCallbackUserData);
		}
		else if (mDirPickCallback)
		{
			mDirPickCallback(mCurrentSelection, mCallbackUserData);
		}
	}
}

//static
void HBFileSelector::loadFile(ELoadFilter filter,
							  HBLoadFileCallback callback, void* user_data)
{
	if (sInstance)
	{
		llwarns << "Call done while a file selector instance already exists !  Aborting."
				<< llendl;
		llassert(false);
	}
	else
	{
		new HBFileSelector(filter, callback, user_data);
	}
}

//static
void HBFileSelector::loadFiles(ELoadFilter filter,
							   HBLoadFilesCallback callback, void* user_data)
{
	if (sInstance)
	{
		llwarns << "Call done while a file selector instance already exists !  Aborting."
				<< llendl;
		llassert(false);
	}
	else
	{
		new HBFileSelector(filter, callback, user_data);
	}
}

//static
void HBFileSelector::saveFile(ESaveFilter filter, std::string suggestion,
							  HBSaveFileCallback callback, void* user_data)
{
	if (sInstance)
	{
		llwarns << "Call done while a file selector instance already exists !  Aborting."
				<< llendl;
		llassert(false);
	}
	else
	{
		new HBFileSelector(filter, suggestion, callback, user_data);
	}
}

//static
void HBFileSelector::pickDirectory(std::string suggestion,
								   HBDirPickCallback callback, void* user_data)
{
	if (sInstance)
	{
		llwarns << "Call done while a file selector instance already exists !  Aborting."
				<< llendl;
		llassert(false);
	}
	else
	{
		new HBFileSelector(suggestion, callback, user_data);
	}
}

//static
void HBFileSelector::onButtonRefresh(void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		if (self->mSavePicker && !self->mInputLine->getText().empty())
		{
			self->mCurrentEntry = self->mInputLine->getText();
		}
		self->mIsDirty = true;
	}
}

//static
void HBFileSelector::onButtonDirLevel(LLUICtrl* ctrl, void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self && ctrl)
	{
		if (self->mDirPicker)
		{
			self->mCurrentEntry.clear();
		}
		std::string operation = ctrl->getValue().asString();
		if (operation == "home")
		{
			self->mCurrentPath = gDirUtilp->getOSUserDir();
			self->isCurrentPathAtRoot();
			self->mIsDirty = true;
		}
		else if (operation == "suggested")
		{
			// *TODO: implement for directory selector as well ?
			if (!self->mDirPicker)
			{
				self->setPathFromContext();
				self->mIsDirty = true;
			}
		}
		else if (operation == "last")
		{
			if (!sLastPath.empty() && LLFile::exists(sLastPath))
			{
				self->mCurrentPath = sLastPath;
				self->mIsDirty = true;
			}
		}
		else if (operation == "root")
		{
			if (!self->isCurrentPathAtRoot())
			{
#if LL_WINDOWS
				self->mCurrentPath.clear();
#else
				self->mCurrentPath = LL_DIR_DELIM_STR;
#endif
				self->isCurrentPathAtRoot();
				self->mIsDirty = true;
			}
		}
		else if (!self->isCurrentPathAtRoot())
		{
			// "level_up" operation
			size_t i = self->mCurrentPath.rfind(LL_DIR_DELIM_CHR);
			if (i != std::string::npos)
			{
				self->mCurrentPath = self->mCurrentPath.substr(0, i);
				self->mIsDirty = true;
			}
#if LL_WINDOWS
			else
			{
				self->mCurrentPath.clear();
				self->mIsDirty = true;
			}
#endif
			self->isCurrentPathAtRoot();
		}
	}
}

//static
void HBFileSelector::onButtonCreate(void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		std::string entry = self->mInputLine->getText();
		if (!entry.empty())
		{
			self->mInputLine->clear();
			self->mCurrentEntry = entry;
		}
		self->mInputLine->setEnabled(true);
		self->mCreatingDirectory = true;
		self->mDirLevelFlyoutBtn->setEnabled(false);
		self->mCreateBtn->setEnabled(false);
		self->mRefreshBtn->setEnabled(false);
		self->mOKBtn->setEnabled(false);
		self->mShowHiddenCheck->setEnabled(false);
		if (self->mShowAllTypesCheck)
		{
			self->mShowAllTypesCheck->setEnabled(false);
		}
		self->mDirectoriesList->setEnabled(false);
		if (self->mFilesList)
		{
			self->mFilesList->setEnabled(false);
		}
		self->setPrompt();
	}
}

static void close_selector(HBFileSelector* floaterp)
{
	floaterp->close();
}

//static
void HBFileSelector::onButtonOK(void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		self->setSelectionData();
		sLastPath = self->mCurrentPath;
		sContextToPathMap[self->mContext] = sLastPath;
#if LL_WINDOWS
		// If we are at the drive selection level, we cannot return a selection
		if (sLastPath.empty())
		{
			self->mFiles.clear();
			self->mCurrentSelection.clear();
		}
#endif
		self->doCallback();
		// We cannot close the floater now, because it would mean destroying
		// this instance while this method is also called for keyboard events
		// occurring at a point its member variables would be used later on,
		// causing a crash. So we instead hide the floater (so that nothing any
		// more can be performed using it) and we register a one-shot idle
		// callback to the close_selector() function.
		self->setVisible(false);
		doOnIdleOneTime(boost::bind(&close_selector, self));
	}
}

//static
void HBFileSelector::onButtonCancel(void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFileSelector::onSelectDirectory(LLUICtrl*, void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		self->mCurrentEntry = self->mInputLine->getText();
		self->setSelectionData();
	}
}

//static
void HBFileSelector::onLevelDown(void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self && self->mDirectoriesList)
	{
		if (self->mDirPicker)
		{
			self->mCurrentEntry.clear();
		}
		LLScrollListItem* item = self->mDirectoriesList->getFirstSelected();
		if (item && item->getColumn(0))
		{
#if LL_WINDOWS
			if (!self->mCurrentPath.empty())
#endif
			{
				self->mCurrentPath += LL_DIR_DELIM_STR;
			}
			self->mCurrentPath += item->getColumn(0)->getValue().asString();
			self->mDirectoriesList->deselectAllItems(true);
			self->isCurrentPathAtRoot();
			self->mIsDirty = true;
		}
	}
}

//static
void HBFileSelector::onSelectFile(LLUICtrl*, void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		self->mCurrentEntry = self->mInputLine->getText();
		self->setSelectionData();
	}
}

//static
void HBFileSelector::onCommitCheckBox(LLUICtrl*, void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self)
	{
		self->mIsDirty = true;
	}
}

//static
bool HBFileSelector::onHandleKeyCallback(KEY key, MASK mask,
										 LLLineEditor* caller, void* user_data)
{
	bool handled = false;

	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self && key == KEY_RETURN && mask == MASK_NONE)
	{
		if (self->mCreatingDirectory)
		{
			self->mCreatingDirectory = false;
			std::string new_dir = self->mInputLine->getText();
			if (!new_dir.empty())
			{
				std::string dir_name = self->mInputLine->getText();
				std::string new_dir = self->mCurrentPath + LL_DIR_DELIM_STR +
									  dir_name;
				LLFile::mkdir(new_dir);
				if (LLFile::isdir(new_dir))
				{
					if (self->mDirPicker)
					{
						// Adopt the new directory as the current entry
						self->mCurrentEntry = dir_name;
					}
					else
					{
						// Change to the newly created directory
						self->mCurrentPath = new_dir;
						self->mDirectoriesList->deselectAllItems(true);
						self->isCurrentPathAtRoot();
						// Restore file suggestion, if any
						self->mInputLine->setText(self->mCurrentEntry);
					}
				}
			}
			if (!self->mSavePicker)
			{
				self->mInputLine->setEnabled(false);
			}
			self->mDirLevelFlyoutBtn->setEnabled(true);
			self->mCreateBtn->setEnabled(true);
			self->mRefreshBtn->setEnabled(true);
			self->mOKBtn->setEnabled(true);
			self->mShowHiddenCheck->setEnabled(true);
			if (self->mShowAllTypesCheck)
			{
				self->mShowAllTypesCheck->setEnabled(true);
			}
			self->mDirectoriesList->setEnabled(true);
			if (self->mFilesList)
			{
				self->mFilesList->setEnabled(true);
			}
			self->setPrompt();
			self->mIsDirty = true;

			handled = true;
		}
		else if (self->mSavePicker)
		{
			self->setSelectionData();
			onButtonOK(self);
			handled = true;
		}
	}

	return handled;
}

//static
void HBFileSelector::onKeystrokeCallback(LLLineEditor* caller, void* user_data)
{
	HBFileSelector* self = (HBFileSelector*)user_data;
	if (self && caller && caller->getEnabled())
	{
		// We must deselect any selected entry if we just typed a new letter,
		// else the selected entry would override what the user entered in the
		// input line whenever a suggested file/dir name corresponds to an
		// existing file/dir...
		if (self->mDirPicker)
		{
			self->mDirectoriesList->deselectAllItems();
		}
		else if (self->mSavePicker && !self->mCreatingDirectory)
		{
			self->mFilesList->deselectAllItems();
		}
	}
}

//static
void HBFileSelector::saveDefaultPaths(const std::string& filename)
{
	std::string fullpath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
														  filename);
	llofstream out(fullpath.c_str());
	if (!out.is_open())
	{
		llwarns << "Unable to open \"" << fullpath
				<< "\" for writing. Default paths not saved." << llendl;
		return;
	}

	llinfos << "Saving default selector paths to: " << fullpath << llendl;

	LLSD data;
	context_map_t::iterator end = sContextToPathMap.end();
	for (S32 context = CONTEXT_DEFAULT; context < CONTEXT_END; ++context)
	{
		std::string path;
		context_map_t::iterator it = sContextToPathMap.find(context);
		if (it != end)
		{
			path = it->second;
		}
		data.set(context, path);
	}
	LLSDSerialize::toPrettyXML(data, out);
	out.close();
}

//static
void HBFileSelector::loadDefaultPaths(const std::string& filename)
{
	std::string fullpath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
														  filename);
	LLSD data;
	llifstream input(fullpath.c_str());
	if (input.is_open())
	{
		llinfos << "Loading default selector paths from: " << fullpath
				<< llendl;
		LLSDSerialize::fromXML(data, input);
	}

	if (data.isUndefined() || !data.isArray())
	{
		llinfos << "Default selector paths file \"" << fullpath
				<< "\" is missing, ill-formed, or simply undefined." << llendl;
		return;
	}

	for (S32 context = CONTEXT_DEFAULT; context < CONTEXT_END; ++context)
	{
		std::string path = data.get(context).asString();
		if (!path.empty())
		{
			sContextToPathMap.emplace(context, path);
		}
	}

	input.close();
}
