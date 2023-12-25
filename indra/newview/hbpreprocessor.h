/**
 * @file hbpreprocessor.h
 * @brief HBPreprocessor class definition
 *
 * HBPreprocessor is a simple sources pre-processor with support for #include,
 * #define (plain defines, no macros)/#undef/#if/#ifdef/#ifndef/#elif/#else/
 * #endif/#warning/#error directives (it also got special #pragma's).
 * It is of course not as complete as boost::wave, but it is about 20 times
 * smaller (when comparing stripped binaries).
 * It can be used to preprocess Lua or LSL source files, for example.
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

#ifndef LL_HBPREPROCESSOR_H
#define LL_HBPREPROCESSOR_H

#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "llerror.h"

struct lua_State;

class HBPreprocessor
{
protected:
	LOG_CLASS(HBPreprocessor);

public:
	enum { PAUSED = -1, FAILURE = 0, SUCCESS = 1 };

	// The #include callback gets passed the include name, mDefaultIncludePath,
	// as well as the 'userdata' that was set on HBPreprocessor construction,
	// and must fill 'buffer' with the contents of the corresponding file or
	// asset. The callback should also modify include_name to prepend it with
	// the full path (so that it can properly be reported in __FILE__). The
	// callback shall return HBPreprocessor::FAILURE on failure to find or load
	// the include, HBPreprocessor::PAUSED if the include asset is not yet
	// available, or HBPreprocessor::SUCCESS on success.
	typedef S32 (*HBPPIncludeCB)(std::string& include_name,
								 const std::string& default_path,
								 std::string& buffer, void* userdata);

	HBPreprocessor(const std::string& file_name, HBPPIncludeCB callback,
				   void* userdata = NULL);
	~HBPreprocessor();

	// Resets the preprocessed data
	void clear();

	// Preprocesses 'sources'. Returns HBPreprocessor::FAILURE on failure,
	// HBPreprocessor::PAUSED when the preprocessing was paused due to a not
	// yet loaded include asset, or HBPreprocessor::SUCCESS on success.
	// The pre-processed sources (up to the last valid line when an error
	// occurred) can be retrieved with getResult() and any error message can be
	// retrieved with getError().
	S32 preprocess(const std::string& sources);

	// This method is to be called after a pause and the include asset that
	// caused it is available. It returns the same results as preprocess()
	// above.
	S32 resume();

	LL_INLINE const std::string& getResult() const	{ return mPreprocessed; }
	LL_INLINE const std::string& getError() const	{ return mErrorMessage; }

	// Returns the line number in the original, non-preprocessed sources, that
	// corresponds to the 'line' in the preprocessed sources, or 0 if there is
	// no match or no mapping.
	LL_INLINE S32 getOriginalLine(S32 line) const
	{
		if (line < 1 || line > (S32)mLineMapping.size())
		{
			return 0;
		}
		return mLineMapping[line - 1];
	}

	// Used to change the preprocessed file name when needed.
	LL_INLINE void setFilename(const std::string& file_name)
	{
		mFilename = file_name;
	}

	// This is an optional callback, for the caller to be informed of warning
	// and error messsages. The callback gets passed the 'userdata' that was
	// set on HBPreprocessor construction.
	typedef void (*HBPPMessageCB)(const std::string& message, bool is_warning,
								  void* userdata);

	// This allows to set the optional error and warning message callback
	LL_INLINE void setMessageCallback(HBPPMessageCB callback)
	{
		mMessageCallback = callback;
	}

	// This is for language-specifc needs, when you want to prevent the use
	// of #define or #undef with special language tokens (e.g. "_G" for Lua).
	LL_INLINE void addForbiddenToken(const char* token)
	{
		mForbiddenTokens.emplace(token);
	}

	// This returns any <path> in the last '#pragma include-from: <path>'
	// directive encountered in the unprocessed sources, or an empty string
	// when no such directive was encountered.
	LL_INLINE const std::string& getDefaultIncludePath() const
	{
		return mDefaultIncludePath;
	}

	// Returns the value for 'name' if the latter is defined, else returns
	// 'name' itself.
	std::string getDefine(const std::string& name) const;

	// Returns true when 'sources' contains preprocessor directives
	static bool needsPreprocessing(const std::string& sources);

private:
	void parsingError(const std::string& error_msg, bool is_warning = false);

	bool isValidToken(const std::string& token);

	// Replaces 'defined(TOKEN)' expressions in 'expr' with either 1 when TOKEN
	// is defined, or 0 when not, then returns the result.
	std::string replaceDefinedInExpr(std::string expr);

	// Replaces in 'line' all defined tokens with their value and returns the
	// result.
	std::string replaceDefinesInLine(const std::string& line);

	bool skipToElseOrEndif(const std::string& buffer, size_t& pos);

	// Method used to evaluate a logical expression using Lua. Note that the
	// "!=", "||", "&&", "!" and "^" operators are automatically translated
	// into their Lua equivalent. This method returns a bool, which is true if
	// the result is not zero or false otherwise (including in case of error).
	// In case of error, the mErrorMessage variables is set.
	bool isExpressionTrue(std::string expression);

	// Used to pre-populate the defines table with the default (and constant
	// during preprocess() execution) defines, namely __DATE__, __TIME__,
	// __AGENT_ID__ and __AGENT_NAME__.
	void setDefaultDefines();

private:
	S32						(*mIncludeCallback)(std::string& include_name,
												const std::string& path,
												std::string& buffer,
												void* userdata);
	void 					(*mMessageCallback)(const std::string& message,
												bool is_warning,
												void* userdata);
	void*					mCallbackUserData;

	lua_State*				mLuaState;

	S32						mCurrentLine;
	S32						mSavedPos;
	// This is the line of the root #include directive in the unprocessed
	// sources. It is used to map #included sources preprocessed lines with
	// unprocessed sources lines.
	S32						mRootIncludeLine;

	std::string				mFilename;
	std::string				mErrorMessage;
	std::string				mSourcesBuffer;
	std::string				mIncludeBuffer;
	std::string				mPreprocessed;
	std::string				mDefaultIncludePath;

	typedef std::map<std::string, std::string> defines_map_t;
	defines_map_t			mDefines;
	std::set<std::string>	mIncludes;
	std::set<std::string>	mForbiddenTokens;
	std::stack<std::string>	mFilenames;
	std::stack<bool>		mIfClauses;
	// This is the line mapping: each entry in this vector corresponds to a
	// line of the preprocessed sources (with line 1 at index 0 of the vector)
	// and contains the corresponding line number in the unprocessed sources
	// (for include files, it is the line number of the root #include directive
	// in the unprocessed sources).
	typedef std::vector<S32> line_map_vec_t;
	line_map_vec_t			mLineMapping;
};

// Extracts one line of text from a buffer: the line shall always be terminated
// with a line feed (true for Linux, macOS and even Windows that also got a
// carriage return before the line feed). I.e. we will not be able to deal with
// the deprecated MacOS (not X) text files format, which ended with a carriage
// return, but this should not be an issue nowadays...
// The line feed (and possible carriage return) is part of the returned 'line'.
// On return, 'pos' is updated to point to the start of the next line in the
// buffer.
// Used also in llpreviewscript.cpp
std::string get_one_line(const std::string& buffer, size_t& pos);

#endif	// LL_HBPREPROCESSOR_H
