/**
 * @file hbexternaleditor.h
 * @brief Utility class to launch an external program for editing a file and
 * tracking changes on the latter.
 *
 * $LicenseInfo:firstyear=2019&license=viewergpl$
 *
 * Copyright (c) 2019, Henri Beauchamp.
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

#ifndef LL_HBEXTERNALEDITOR_H
#define LL_HBEXTERNALEDITOR_H

#include "llstring.h"

class LLLiveFile;
class LLProcessLauncher;

class HBExternalEditor
{
	friend class HBEditorLiveFile;

protected:
	LOG_CLASS(HBExternalEditor);

public:
	typedef void (*HBExternalEditorFileChangedCB)(const std::string& filename,
												  void* userdata);
	HBExternalEditor(HBExternalEditorFileChangedCB callback,
					 void* userdata = NULL,
					 bool orphanize_on_destroy = false);
	~HBExternalEditor();

	// Call with the name of the file to edit and watch, as well as an optional
	// command line (with "%s" as the string argument symbol that will be
	// replaced with the filename). E.g.: /usr/bin/nedit %s
	// Returns true on success, false on error (error message set accordingly).
	bool open(const std::string& filename,
			  std::string command_line = LLStringUtil::null);

	// Call to attempt to kill the external editor (also closes the live file)
	void kill();

	// Returns true when the external editor is still running, or when we know
	// for sure that the editor is detached from the original (and now gone)
	// launched process, which happens when we launch a MIME wrapper launcher
	// instead of the actual editor.
	bool running();

	// Returns the last error message.
	LL_INLINE const std::string& getErrorMessage()	{ return mErrorMessage; }

	// Call this when planning to update the file yourself and not wanting
	// to get notified uselessly about it via the changed callback.
	LL_INLINE void ignoreNextUpdate()				{ mIgnoreNextUpdate = true; }

	std::string getFilename();

private:
	// For use by HBEditorLiveFile only
	void callChangedCallback(const std::string& filename);

private:
	void				(*mFiledChangedCallback)(const std::string& filename,
												 void* userdata);
	void*				mUserData;
	LLProcessLauncher*	mProcess;
	LLLiveFile*			mEditedFile;
	std::string			mErrorMessage;
	bool				mOrphanizeOnDestroy;
	bool				mIgnoreNextUpdate;
	bool				mEditorIsDetached;
};

#endif	// LL_HBEXTERNALEDITOR_H
