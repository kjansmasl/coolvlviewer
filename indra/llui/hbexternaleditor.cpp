/**
 * @file hbexternaleditor.cpp
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

#include "linden_common.h"

#include "hbexternaleditor.h"

#include "lllivefile.h"
#include "llprocesslauncher.h"
#include "lltrans.h"
#include "llui.h"				// For LLUI::sConfigGroup

///////////////////////////////////////////////////////////////////////////////
// HBEditorLiveFile class, for edited file live tracking
///////////////////////////////////////////////////////////////////////////////

class HBEditorLiveFile final : public LLLiveFile
{
public:
	HBEditorLiveFile(HBExternalEditor* editor, const std::string& filename)
	:	LLLiveFile(filename, 1.f),
		mEditor(editor)
	{
	}

	~HBEditorLiveFile() override
	{
	}

protected:
	bool loadFile() override
	{
		if (mEditor)
		{
			mEditor->callChangedCallback(filename());
		}
		return true;
	}

private:
	HBExternalEditor* mEditor;
};

///////////////////////////////////////////////////////////////////////////////
// HBExternalEditor class proper
///////////////////////////////////////////////////////////////////////////////

HBExternalEditor::HBExternalEditor(HBExternalEditorFileChangedCB callback,
								   void* userdata, bool orphanize_on_destroy)
:	mFiledChangedCallback(callback),
	mUserData(userdata),
	mIgnoreNextUpdate(false),
	mEditorIsDetached(false),
	mOrphanizeOnDestroy(orphanize_on_destroy),
	mProcess(NULL),
	mEditedFile(NULL)
{
}

HBExternalEditor::~HBExternalEditor()
{
	if (mEditedFile)
	{
		delete mEditedFile;
	}
	if (mProcess)
	{
		if (mOrphanizeOnDestroy)
		{
			mProcess->orphan();
		}
		delete mProcess;
	}
}

void HBExternalEditor::callChangedCallback(const std::string& filename)
{
	if (!mIgnoreNextUpdate && mFiledChangedCallback)
	{
		mFiledChangedCallback(filename, mUserData);
	}
	mIgnoreNextUpdate = false;
}

bool HBExternalEditor::open(const std::string& filename, std::string cmd)
{
	if (!LLFile::isfile(filename))
	{
		mErrorMessage = LLTrans::getString("file_not_found") + " " + filename;
		llwarns << mErrorMessage << llendl;
		return false;
	}

	mEditorIsDetached = false;
	if (cmd.empty())
	{
		cmd = LLUI::sConfigGroup->getString("ExternalEditor");
	}
	if (cmd.empty())
	{
#if LL_LINUX
		llwarns << "Could not find a configured editor; trying 'xdg-open'. This is suboptimal because the state of the editor it will launch cannot be tracked. Please, consider configuring the \"ExternalEditor\" setting."
				<< llendl;
		// *TODO: try every PATH element, in case xdg-open is not in /usr/bin ?
		cmd = "/usr/bin/xdg-open %s";
		mEditorIsDetached = true;
#elif LL_DARWIN
		llwarns << "Could not find a configured editor; trying 'open'. This is suboptimal because the state of the editor it will launch cannot be tracked. Please, consider configuring the \"ExternalEditor\" setting."
				<< llendl;
		// *TODO: try every PATH element, in case open is not in /usr/bin ?
		cmd = "/usr/bin/open -e %s";
		mEditorIsDetached = true;
#elif LL_WINDOWS
		llwarns << "Could not find a configured editor; trying 'notepad.exe'."
				<< llendl;
		cmd = "\"C:\\Windows\\System32\\notepad.exe\" \"%s\"";
#endif
	}
	LLStringUtil::trim(cmd);
	if (cmd.empty())
	{
		mErrorMessage = LLTrans::getString("no_valid_command");
		llwarns << mErrorMessage << llendl;
		return false;
	}

	// Split the command line between program file name and arguments
	std::string prg;
	size_t i;
	if (cmd[0] == '"')
	{
		// Starting with a quoted program name, as often seen under Windows,
		// because of spaces in the path.
		i = cmd.find('"', 1);	// Find the matching closing quote
		if (i == std::string::npos)
		{
			mErrorMessage = LLTrans::getString("bad_quoting");
			llwarns << mErrorMessage << llendl;
			return false;
		}
		prg = cmd.substr(1, i - 1);
		cmd = cmd.substr(i + 1);
	}
	else
	{
		i = cmd.find(' ', 1);	// Find the first space
		if (i == std::string::npos)
		{
			// No argument, just a program...
			prg = cmd;
			cmd.clear();
		}
		else
		{
			prg = cmd.substr(0, i);
			cmd = cmd.substr(i + 1);
		}
	}
	if (cmd.find("%s") == std::string::npos)
	{
		// Add the filename if absent from the arguments
#if LL_WINDOWS
		cmd += " \"%s\"";
#else
		cmd += " %s";
#endif
	}
	LLStringUtil::trimHead(cmd);

	if (!LLFile::isfile(prg))
	{
		mErrorMessage = LLTrans::getString("program_not_found") + " " + prg;
		llwarns << mErrorMessage << llendl;
		return false;
	}

	llinfos << "Using external editor command line: " << prg << " " << cmd
			<< llendl;

	if (mEditedFile)
	{
		delete mEditedFile;
		mEditedFile = NULL;
	}
	// Watch as live file only if we got a "file changed" event callback
	if (mFiledChangedCallback)
	{
		mEditedFile = new HBEditorLiveFile(this, filename);
		mEditedFile->addToEventTimer();
	}

	std::vector<std::string> tokens;
	LLStringUtil::getTokens(cmd, tokens, " ");
	if (mProcess)
	{
		if (mOrphanizeOnDestroy)
		{
			mProcess->orphan();
		}
		else
		{
			mProcess->kill();
		}
		mProcess->clearArguments();
		mProcess->setWorkingDirectory("");
	}
	else
	{
		mProcess = new LLProcessLauncher();
	}
	mProcess->setExecutable(prg);
	for (U32 i = 0, count = tokens.size(); i < count; ++i)
	{
		std::string& parameter = tokens[i];
		if (!parameter.empty())
		{
#if LL_LINUX || LL_DARWIN
			// Under POSIX operating systems, arguments for execv() are passed
			// in the argv array and none need quoting; much to the contrary
			// since quotes would cause the path to be considered relative and
			// be prefixed with the working directory path, which is not what
			// we want here !
			LLStringUtil::replaceString(parameter, "\"%s\"", filename);
#endif
			LLStringUtil::replaceString(parameter, "%s", filename);
			mProcess->addArgument(parameter);
		}
	}

	if (mProcess->launch() != 0)
	{
		mErrorMessage = LLTrans::getString("command_failed") + " " + prg +
						" " + cmd;
		llwarns << mErrorMessage << llendl;
		kill();
		return false;
	}

	// Opening the file in the external editor caused it to be touched and we
	// do not want to trigger a "file changed" event for this...
	mIgnoreNextUpdate = true;

	return true;
}

void HBExternalEditor::kill()
{
	if (mEditedFile)
	{
		delete mEditedFile;
		mEditedFile = NULL;
	}
	if (mProcess)
	{
		if (mEditorIsDetached)
		{
			llwarns << "Cannot kill a detached editor process..." << llendl;
		}
		delete mProcess;
		mProcess = NULL;
	}
}

bool HBExternalEditor::running()
{
	return mProcess && (mEditorIsDetached || mProcess->isRunning());
}

std::string HBExternalEditor::getFilename()
{
	return mEditedFile ? mEditedFile->filename() : LLStringUtil::null;
}
