/**
 * @file hbpreprocessor.cpp
 * @brief HBPreprocessor class implementation
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

#include "llviewerprecompiledheaders.h"

#include <stdlib.h>					// For atof() and atoi()

#include "lua.hpp"

#include "hbpreprocessor.h"

#include "lldir.h"
#include "lltimer.h"

#include "llagent.h"
#include "llappviewer.h"			// For gSecondLife, gViewerVersion*
#include "llviewercontrol.h"

// Helper functions

std::string get_one_line(const std::string& buffer, size_t& pos)
{
	std::string line;
	char c;
	size_t len = buffer.length();
	while (pos < len)
	{
		c = buffer[pos++];
		line += c;
		if (c == '\n')
		{
			break;
		}
	}
	return line;
}

// Skips all spaces/tabs in 'line' and changes 'pos' to point on the first
// non-spacing character.
static void skip_spacing(const std::string& line, size_t& pos)
{
	size_t len = line.length();
	while (pos < len)
	{
		char c = line[pos];
		if (c != ' ' && c != '\t')
		{
			break;
		}
		++pos;
	}
}

// Attempts to find a preprocessor directive in 'line': valid directives shall
// be prefixed with a '#' that must be the first non-spacing character in
// 'line', the '#' itself may be followed with spacing characters before the
// directive name. When the directive accepts arguments, they must be separated
// from its name with spacing characters.
// This function returns false when it cannot find a directive-like statement
// in 'line'. When a potential directive is found, it returns true with the
// directive name in 'directive' and its argument in 'argument' (empty string
// when no argument is found).
static bool is_directive(const std::string& line, std::string& directive,
						 std::string& argument, bool directive_only = false)
{
	// Skip all spaces/tabs till the start of a word.
	size_t i = 0;
	skip_spacing(line, i);

	size_t len = line.length();
	if (i >= len || line[i] != '#')
	{
		return false;
	}

	++i;					// Skip the '#'...
	skip_spacing(line, i);	// ... and any spacing till the start of a word.

	// Get the directive name, which must be separated with spacing from its
	// arguments, when present.
	directive.clear();
	while (i < len)
	{
		char c = line[i++];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			break;
		}
		directive += c;
	}

	if (directive_only)
	{
		return true;
	}

	// Skip arguments spacing, possibly
	skip_spacing(line, i);

	// If any argument is present, get it
	argument.clear();
	if (i + 1 < len)
	{
		argument = line.substr(i);
		// Strip trailing spacing or end of line characters
		len = argument.length();
		// Note: size_t is most often an unsigned int, so we cannot use i here.
		S32 j = len;
		while (--j >= 0)
		{
			char c = argument[j];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			{
				break;
			}
		}
		if (j < 0)
		{
			argument.clear();
		}
		else if ((size_t)j + 1 < len)
		{
			argument.erase(j + 1);
		}
	}

	return true;
}

// Attempts to get a define name and the expression/value following it in
// 'line', i.e. something in the form "DEFINE_NAME expression or value".
// Note that macros (e.g. "MACRO_NAME(x, y, z)") are not accepted as valid
// defines.
static std::string get_define_and_value(const std::string& line,
										std::string& value)
{
	size_t len = line.length();

	// Get the define name
	std::string define;
	size_t i = 0;
	while (i < len)
	{
		char c = line[i++];
		if (c != '_' && !LLStringOps::isAlnum(c))
		{
			break;
		}
		define += c;
	}

	// Skip all spaces/tabs
	skip_spacing(line, i);
	if (i < len)
	{
		value = line.substr(i);
	}
	else
	{
		value.clear();
	}

	return define;
}

// HBPreprocessor class proper

HBPreprocessor::HBPreprocessor(const std::string& file_name,
							   HBPPIncludeCB callback, void* userdata)
:	mFilename(file_name),
	mCurrentLine(0),
	mRootIncludeLine(0),
	mSavedPos(0),
	mIncludeCallback(callback),
	mMessageCallback(NULL),
	mCallbackUserData(userdata)
{
	if (!mIncludeCallback)
	{
		llerrs << "A non-NULL #include callback must be passed !" << llendl;
	}

	// Do not allow to define defined()...
	mForbiddenTokens.emplace("defined");
	// These are special, read-only defines:
	mForbiddenTokens.emplace("__DATE__");
	mForbiddenTokens.emplace("__TIME__");
	mForbiddenTokens.emplace("__FILE__");
	mForbiddenTokens.emplace("__LINE__");
	mForbiddenTokens.emplace("__AGENT_ID__");
	mForbiddenTokens.emplace("__AGENT_NAME__");
	mForbiddenTokens.emplace("__VIEWER_NAME__");
	mForbiddenTokens.emplace("__VIEWER_VERSION__");
	mForbiddenTokens.emplace("__VIEWER_VERNUM__");

	mLuaState = luaL_newstate();
	if (!mLuaState)
	{
		llwarns << "Failure to allocate a new Lua state !" << llendl;
		llassert(false);
	}
}

HBPreprocessor::~HBPreprocessor()
{
	if (mLuaState)
	{
		lua_settop(mLuaState, 0);
		lua_close(mLuaState);
	}
}

bool HBPreprocessor::isValidToken(const std::string& token)
{
	if (token.empty() || mForbiddenTokens.count(token))
	{
		return false;
	}

	for (size_t i = 0, len = token.length(); i < len; ++i)
	{
		char c = token[i];
		if (i > 0 && LLStringOps::isDigit(c))
		{
			continue;
		}
		if (c != '_' && !LLStringOps::isAlpha(c))
		{
			return false;
		}
	}

	return true;
}

bool HBPreprocessor::isExpressionTrue(std::string expression)
{
	LL_DEBUGS("Preprocessor") << "Evaluating expression: " << expression
							  << LL_ENDL;
	if (!mLuaState)
	{
		return atof(expression.c_str()) != 0.f;
	}

	lua_settop(mLuaState, 0);	// Empty the stack

	// Translate C operators into Lua ones
	LLStringUtil::replaceString(expression, "!=", "~=");
	LLStringUtil::replaceString(expression, "||", " or ");
	LLStringUtil::replaceString(expression, "&&", " and ");
	LLStringUtil::replaceString(expression, "!", " not ");
	LLStringUtil::replaceString(expression, "^", "~");
	LL_DEBUGS("Preprocessor") << "Lua translated expression: " << expression
							  << LL_ENDL;
	// Use the expression and assign it to a Lua global variable.
	expression = "V_EVAL_PP_EXPRESSION=" + expression;
	if (luaL_loadstring(mLuaState, expression.c_str()) != LUA_OK)
	{
		LL_DEBUGS("Preprocessor") << "Lua error loading expression: "
								  << lua_tostring(mLuaState, -1) << LL_ENDL;
		return false;
	}
	if (lua_pcall(mLuaState, 0, LUA_MULTRET, 0) != LUA_OK)
	{
		LL_DEBUGS("Preprocessor") << "Lua error evaluationg expression: "
								  << lua_tostring(mLuaState, -1) << LL_ENDL;
		return false;
	}

	// Put the variable contents on the Lua stack
	lua_getglobal(mLuaState, "V_EVAL_PP_EXPRESSION");

	bool success;
	// WARNING: under Lua 0 is true, not false, thus why we *must* check the
	// type of the value resulting from the expression evaluation and take
	// appropriate decisions.
	int type = lua_type(mLuaState, -1);
	switch (type)
	{
		case LUA_TNIL:
			success = false;
			break;

		case LUA_TBOOLEAN:
			success = lua_toboolean(mLuaState, -1);
			break;

		case LUA_TNUMBER:
			success = lua_tonumber(mLuaState, -1) != 0.f;
			break;

		case LUA_TSTRING:
			// Note: what if the string is "0.0", for example ?... Should we
			// check for this ?
			success = strlen(lua_tostring(mLuaState, -1)) != 0;
			break;

		default:
			// Tables, etc... Let's consider "something" is "true"
			success = true;
	}

	LL_DEBUGS("Preprocessor") << "Expression is "
							  << (success ? "true" : "false") << LL_ENDL;
	return success;
}

std::string HBPreprocessor::getDefine(const std::string& name) const
{
	if (name == "__FILE__")
	{
		return mFilenames.empty() ? "" : mFilenames.top();
	}
	else if (name == "__LINE__")
	{
		return llformat("%d", mCurrentLine);
	}
	else if (!name.empty())
	{
		defines_map_t::const_iterator it = mDefines.find(name);
		if (it != mDefines.end())
		{
			return it->second;
		}
	}

	return name;
}

std::string HBPreprocessor::replaceDefinedInExpr(std::string expr)
{
	if (expr.empty())
	{
		// It is OK to return "0" (and not an Lua "false"), because even though
		// 0 == true under Lua, we retrieve this value as a number, not as a
		// boolean, and we do properly consider a 0 number as "false".
		// Note that this will work as well for atof() when Lua is not used.
		return "0";
	}

	LL_DEBUGS("Preprocessor") << "Raw expression: " << expr << LL_ENDL;

	std::string token;
	size_t i;
	while ((i = expr.find("defined(")) != std::string::npos)
	{
		size_t j = expr.find(')', i);
		if (j == std::string::npos)
		{
			parsingError("No matching closing parenthesis for defined()");
			return "";
		}
		
		token = expr.substr(i + 8, j - i - 8);
		LLStringUtil::replaceString(token, "\t", " ");
		LLStringUtil::trim(token);
		token = mDefines.count(token) ? "true" : "false";
		expr = expr.substr(0, i) + token + expr.substr(j + 1);
	}

	LL_DEBUGS("Preprocessor") << "Processed expression: " << expr << LL_ENDL;

	return expr;
}

std::string HBPreprocessor::replaceDefinesInLine(const std::string& line)
{
	if (line.empty() || line == "\n" || line == "\r\n")
	{
		return line;
	}

	LL_DEBUGS("Preprocessor") << "Unprocessed line: "
							  << line.substr(0, line.find_first_of("\n\r"))
							  << LL_ENDL;

	std::string result, word, value;
	size_t len = line.length();
	size_t pos = 0;
	bool in_quotes = false;
	bool in_double_quotes = false;
	bool escaped = false;
	bool valid_word_char;
	while (pos < len)
	{
		char c = line[pos++];

		valid_word_char = LLStringUtil::isPartOfWord(c);
		if (!valid_word_char)
		{
			if (!word.empty())
			{
				result += getDefine(word);
				word.clear();
			}
			result += c;
		}

		if (c == '\\')				// Invalid char, so already added to result
		{
			escaped = !escaped;
			continue;
		}

		if (escaped)
		{
			if (valid_word_char)	// Valid char, so not yet added to result
			{
				result += c;
			}
			escaped = false;
			continue;
		}

		if (c == '\'')				// Invalid char, so already added to result
		{
			in_quotes = !in_quotes && !in_double_quotes;
			continue;
		}
		if (c == '"')				// Invalid char, so already added to result
		{
			in_double_quotes = !in_double_quotes && !in_quotes;
			continue;
		}

		if (valid_word_char)
		{
			if (!in_quotes && !in_double_quotes)
			{
				word += c;
			}
			else
			{
				result += c;
			}
		}
	}

	if (!word.empty())
	{
		result += getDefine(word);
	}

	LL_DEBUGS("Preprocessor") << "Preprocessed line: "
							  << result.substr(0, result.find_first_of("\n\r"))
							  << LL_ENDL;

	return result;
}

void HBPreprocessor::parsingError(const std::string& message, bool is_warning)
{
	std::string msg =
		llformat("File: %s - Line: %d - ",
				 mFilenames.empty() ? "?" : mFilenames.top().c_str(),
				 mCurrentLine) + message;
	if (!is_warning)
	{
		mErrorMessage = msg;
	}
	llwarns << msg << llendl;
	if (mMessageCallback)
	{
		mMessageCallback(msg, is_warning, mCallbackUserData);
	}
}

bool HBPreprocessor::skipToElseOrEndif(const std::string& buffer, size_t& pos)
{
	S32 level = 0;					// Number of nested #if* directives
	size_t len = buffer.length();
	std::string line, directive, argument;
	while (pos < len)
	{
		size_t old_pos = pos;		// Keep old position for #elif

		line = get_one_line(buffer, pos);
		if (line.empty())			// This should never happen...
		{
			parsingError("Internal error in get_one_line(): empty line returned");
			return false;
		}

		++mCurrentLine;

		if (!is_directive(line, directive, argument))
		{
			continue;
		}

		// Check for #include boundary
		if (directive == "endinclude")
		{
			LL_DEBUGS("Preprocessor") << "Found #endinclude " << argument
									  << LL_ENDL;
			parsingError("Matching #endif not found.");
			return false;
		}

		if (level == 0)				// Ignore all deeper levels
		{
			if (directive == "else" || directive == "elif")
			{
				LL_DEBUGS("Preprocessor") << "Found a #" << directive
										  << " " << argument << LL_ENDL;
				// Only take them into account when the matching #if failed
				if (mIfClauses.empty())	// This should never happen...
				{
					parsingError("Internal error: #if clauses stack empty.");
					return false;
				}
				if (!mIfClauses.top())
				{
					// We found the matching #else or #elif and must execute
					// (what follows) it.
					if (directive == "elif")
					{
						// We must evaluate the #elif, so restore its position
						pos = old_pos;
						--mCurrentLine;
					}
					return true;
				}
			}
			else if (directive == "endif")
			{
				LL_DEBUGS("Preprocessor") << "Found a #endif" << LL_ENDL;
				if (mIfClauses.empty())	// This should never happen...
				{
					parsingError("Internal error: #if clauses stack empty.");
					return false;
				}
				mIfClauses.pop();
				return true;
			}
		}

		if (directive.find("if") == 0)
		{
			LL_DEBUGS("Preprocessor") << "Found a new #" << directive
									  << ", incrementing level." << LL_ENDL;
			++level;
		}
		else if (directive == "endif")
		{
			LL_DEBUGS("Preprocessor") << "Found an #endif, decrementing level."
									  << LL_ENDL;
			--level;
			if (level < 0)
			{
				parsingError("Found #endif without matching #if");
				return false;
			}
		}
	}

	parsingError("Matching #endif not found.");
	return false;
}

void HBPreprocessor::clear()
{
	// Reset our member variables
	mPreprocessed.clear();
	mLineMapping.clear();
	mIncludeBuffer.clear();
	mIncludes.clear();
	mDefaultIncludePath.clear();
	mErrorMessage.clear();
	while (!mIfClauses.empty())
	{
		mIfClauses.pop();
	}
	while (!mFilenames.empty())
	{
		mFilenames.pop();
	}
	mCurrentLine = mRootIncludeLine = mSavedPos = 0;
}

// Note that __FILE__ and __LINE__ are "dynamic" (they change during the
// preprocessing of the sources) and not set by setDefaultDefines(), but
// instead replaced explicitely in replaceDefinesInLine().
void HBPreprocessor::setDefaultDefines()
{
	std::string format, temp;
	mDefines.clear();
	// Get the local time
	struct tm* internal_time;
	time_t local_time = computer_time();
	internal_time = local_time_to_tm(local_time);
	// Format the date, following the user's preferences
	format = "\"" + gSavedSettings.getString("ShortDateFormat") + "\"";
	timeStructToFormattedString(internal_time, format, temp);
	mDefines["__DATE__"] = temp;
	// Format the time, following the user's preferences
	format = "\"" + gSavedSettings.getString("LongTimeFormat") + "\"";
	timeStructToFormattedString(internal_time, format, temp);
	mDefines["__TIME__"] = temp;
	mDefines["__AGENT_ID__"] = "\"" + gAgentID.asString() + "\"";
	gAgent.getName(temp);
	mDefines["__AGENT_NAME__"] = "\"" + temp + "\"";
	mDefines["__VIEWER_NAME__"] = "\"" + gSecondLife + "\"";
	mDefines["__VIEWER_VERSION__"] = "\"" + gViewerVersionString + "\"";
	mDefines["__VIEWER_VERNUM__"] = llformat("%lu", gViewerVersionNumber);
}

S32 HBPreprocessor::preprocess(const std::string& sources)
{
	clear();

	// Set current filename and line number:
	mFilenames.emplace("\"" + mFilename + "\"");

	// Set the default defines
	setDefaultDefines();

	// Initialise the sources buffer and position
	mSourcesBuffer = sources;

	return resume();
}

S32 HBPreprocessor::resume()
{
	// Preprocessing is always enabled on resume (since, even if paused, it was
	// the result of an #include interpretation, and it was therefore enabled
	// when it happened).
	bool enabled = true;

	std::string line, directive, argument;
	size_t pos = mSavedPos;
	size_t len = mSourcesBuffer.length();
	while (pos < len)
	{
		mSavedPos = pos;	// Preserve in case of PAUSED event
		++mCurrentLine;

		line = get_one_line(mSourcesBuffer, pos);
		if (line.empty())	// This should never happen...
		{
			parsingError("Internal error in get_one_line(): empty line returned");
			return FAILURE;
		}

		if (mCurrentLine == 1 && line.find("#!") == 0)
		{
			// This is a shebang line, just ignore it and continue
			continue;
		}

		// If it is not a pre-processor directive, just store the line in the
		// processed buffer and continue with next line.
		if (!is_directive(line, directive, argument))
		{
			if (enabled)
			{
				// Proceed to replace all #defined tokens with their value in
				// that line
				line = replaceDefinesInLine(line);
			}
			mPreprocessed += line;
			if (mFilenames.size() == 1)
			{
				// We are processing the original sources: map this newly
				// inserted preprocessed line with the line in the said
				// sources.
				mLineMapping.push_back(mCurrentLine);
			}
			else
			{
				// We are processing an #include: map this newly inserted
				// preprocessed line with the line of the root #include
				// directive in the original (unprocessed) sources.
				mLineMapping.push_back(mRootIncludeLine);
			}
			continue;
		}

		if (directive == "endinclude")
		{
			LL_DEBUGS("Preprocessor") << "Found #endinclude " << argument
									  << LL_ENDL;
			mCurrentLine = atoi(argument.c_str());
			if (!enabled)
			{
				parsingError("Missing directive '#pragma preprocessor-on' at end of file");
				return FAILURE;
			}
			else if (mFilenames.size() < 2)
			{
				parsingError("Unexpected directive #endinclude " + argument);
				return FAILURE;
			}
			else if (mCurrentLine <= 0)
			{
				parsingError("Invalid directive #endinclude " + argument);
				return FAILURE;
			}
			mFilenames.pop();
			continue;
		}

		if (directive == "pragma")
		{
			LL_DEBUGS("Preprocessor") << "Found #pragma " << argument
									  << LL_ENDL;
			if (argument.find("preprocessor-on") == 0)
			{
				LL_DEBUGS("Preprocessor") << "Preprocessing enabled"
										  << LL_ENDL;
				enabled = true;
			}
			else if (enabled && argument.find("preprocessor-off") == 0)
			{
				LL_DEBUGS("Preprocessor") << "Preprocessing disabled"
										  << LL_ENDL;
				enabled = false;
			}
			else if (enabled && argument.length() > 14 && 
					 argument.find("include-from: ") == 0)
			{
				mDefaultIncludePath = argument.substr(14);
				LL_DEBUGS("Preprocessor") << "Default include path set to: "
										  << mDefaultIncludePath << LL_ENDL;
			}

			continue;
		}

		// If preprocessing is disabled, simply consider the directive is
		// a normal line
		if (!enabled)
		{
			mPreprocessed += line;
			if (mFilenames.size() == 1)
			{
				// We are processing the original sources: map this newly
				// inserted preprocessed line with the line in the said
				// sources.
				mLineMapping.push_back(mCurrentLine);
			}
			else
			{
				// We are processing an #include: map this newly inserted
				// preprocessed line with the line of the root #include
				// directive in the original (unprocessed) sources.
				mLineMapping.push_back(mRootIncludeLine);
			}
			continue;
		}

		if (directive == "include")
		{
			LL_DEBUGS("Preprocessor") << "Found: #include " << argument
									  << LL_ENDL;
			// Check for the presence of quotes or angle brackets
			size_t i = argument.length() - 1;
			if (i < 2 ||
				(argument[0] != '"' && argument[0] != '<') ||
				(argument[i] != '"' && argument[i] != '>'))
			{
				parsingError("Invalid #include name provided: " + argument);
				return FAILURE;
			}
			// Strip off the quotes or angle brackets
			argument = argument.substr(1, i - 1);
			// Only actually include the file if it was not already included
			// (this avoids infinite loops).
			if (!mIncludes.count(argument))
			{
				// Let our caller deal with the include file retrieval and
				// recover the text it contains.
				mIncludeBuffer.clear();
				line = argument;	// 'line' will be modified with full path
				S32 result = mIncludeCallback(line, mDefaultIncludePath,
											  mIncludeBuffer,
											  mCallbackUserData);
				if (result == FAILURE)
				{
					parsingError("Failure to #include: " + argument);
					return FAILURE;
				}
				else if (result == PAUSED)
				{
					LL_DEBUGS("Preprocessor") << "Pausing until asset is available for #include: "
											  << argument << LL_ENDL;
					--mCurrentLine;	// We will retry the #include on resume()
					return PAUSED;
				}
				// Remember that this file has been successfully included
				mIncludes.emplace(argument);
				// If we are not already processing an include, remember the
				// line of this root #include directive, for sources lines
				// mapping.
				if (mFilenames.size() == 1)
				{
					mRootIncludeLine = mCurrentLine;
				}
				// Make sure there is a trailing line feed
				if (mIncludeBuffer[mIncludeBuffer.length() - 1] != '\n')
				{
					mIncludeBuffer += '\n';
				}
				// Add a special boundary directive at the end of the included
				// file block to allow tracking the filename and line number.
				mIncludeBuffer += llformat("#endinclude %d\n", mCurrentLine);
				// Push the name of the included file on the stack and set
				// the current line to 0, since this is what we are going to
				// process at the next loop.
				mFilenames.emplace("\"" + line + "\"");
				mCurrentLine = 0;
				// Replace our buffer with the included file followed with
				// whatever is left to process in the original buffer.
				mSourcesBuffer = mIncludeBuffer + mSourcesBuffer.substr(pos);
				// Continue processing from the start of our new buffer
				len = mSourcesBuffer.length();
				pos = 0;
			}
			else
			{
				LL_DEBUGS("Preprocessor") << "Skipping inclusion of already #included file: "
										  << argument << LL_ENDL;
			}
		}
		else if (directive == "define")
		{
			argument = get_define_and_value(argument, line);
			LL_DEBUGS("Preprocessor") << "Found: #define " << argument << " "
									  << line << LL_ENDL;
			if (!isValidToken(argument))
			{
				parsingError("Cannot define '" + argument +
							 "': invalid token.");
				return FAILURE;
			}
			if (mDefines.count(argument))
			{
				parsingError("Cannot redefine '" + argument +
							 "' which is already defined.");
				return FAILURE;
			}
			mDefines[argument] = line;
		}
		else if (directive == "undef")
		{
			LL_DEBUGS("Preprocessor") << "Found: #undef " << argument
									  << LL_ENDL;
			if (!isValidToken(argument))
			{
				parsingError("Cannot undefine '" + argument +
							 "': invalid token.");
				return FAILURE;
			}
			defines_map_t::iterator it = mDefines.find(argument);
			if (it != mDefines.end())
			{
				mDefines.erase(it);
			}
		}
		else if (directive == "ifdef")
		{
			LL_DEBUGS("Preprocessor") << "Found: #ifdef " << argument
									  << LL_ENDL;
			if (mDefines.count(argument))
			{
				mIfClauses.push(true);
			}
			else
			{
				mIfClauses.push(false);
				// Condition not met, skip lines till we find a #elif, #else or
				// #endif (and resume the flow one line past them)
				LL_DEBUGS("Preprocessor") << "Condition not met." << LL_ENDL;
				if (!skipToElseOrEndif(mSourcesBuffer, pos))
				{
					return FAILURE;
				}
			}
		}
		else if (directive == "ifndef")
		{
			LL_DEBUGS("Preprocessor") << "Found: #ifndef " << argument
									  << LL_ENDL;
			if (mDefines.count(argument))
			{
				mIfClauses.push(false);
				// Condition not met, skip lines till we find a #elif, #else or
				// #endif (and resume the flow one line past them)
				LL_DEBUGS("Preprocessor") << "Condition not met." << LL_ENDL;
				if (!skipToElseOrEndif(mSourcesBuffer, pos))
				{
					return FAILURE;
				}
			}
			else
			{
				mIfClauses.push(true);
			}
		}
		else if (directive == "if" || directive == "elif")
		{
			LL_DEBUGS("Preprocessor") << "Found: #" << directive << " "
									  << argument << LL_ENDL;
			// Proceed to replace all #defined tokens with their value in the
			// expression
			argument = replaceDefinedInExpr(argument);
			argument = replaceDefinesInLine(argument);
			bool met = !argument.empty() && isExpressionTrue(argument);
			if (directive == "elif")
			{
				if (mIfClauses.empty())
				{
					parsingError("#elif without matching #if");
					return FAILURE;
				}
				mIfClauses.pop();
			}
			mIfClauses.push(met);
			if (!met)
			{
				// Condition not met, skip lines till we find a #elif, #else or
				// #endif (and resume the flow one line past them)
				LL_DEBUGS("Preprocessor") << "Condition not met." << LL_ENDL;
				if (!skipToElseOrEndif(mSourcesBuffer, pos))
				{
					return FAILURE;
				}
			}
		}
		else if (directive == "else")
		{
			LL_DEBUGS("Preprocessor") << "Found: #else" << LL_ENDL;
			if (mIfClauses.empty())
			{
				parsingError("#else without matching #if");
				return false;
			}
			if (!skipToElseOrEndif(mSourcesBuffer, pos))
			{
				return FAILURE;
			}
		}
		else if (directive == "endif")
		{
			LL_DEBUGS("Preprocessor") << "Found: #endif" << LL_ENDL;
			if (mIfClauses.empty())
			{
				parsingError("#endif without matching #if");
				return FAILURE;
			}
			mIfClauses.pop();
		}
		else if (directive == "warning")
		{
			LL_DEBUGS("Preprocessor") << "Found: #warning " << argument
									  << LL_ENDL;
			parsingError("#warning: " + argument, true);
		}
		else if (directive == "error")
		{
			LL_DEBUGS("Preprocessor") << "Found: #error " << argument
									  << LL_ENDL;
			parsingError("#error: " + argument);
			return FAILURE;
		}
		else
		{
			parsingError("Unknown pre-processor directive: " + directive);
			return FAILURE;
		}
	}

	if (!mIfClauses.empty())
	{
		parsingError("Missing #endif");
		return FAILURE;
	}

	LL_DEBUGS("Preprocessor") << "Preprocessed sources:\n" << mPreprocessed
							  << LL_ENDL;

	return SUCCESS;
}

//static
bool HBPreprocessor::needsPreprocessing(const std::string& sources)
{
	size_t pos = 0;
	size_t len = sources.length();
	std::string line, directive, argument;
	while (pos < len)
	{
		line = get_one_line(sources, pos);
		if (is_directive(line, directive, argument, true))
		{
			// Test for known directives, most likely ones first, ignoring
			// #elif, #else and #endif, since there must be an #if* appearing
			// before them anyway...
			if (directive == "include" || directive == "define" ||
				directive == "ifdef" || directive == "ifndef" ||
				directive == "if" || directive == "undef" ||
				directive == "warning" || directive == "error" ||
				directive == "pragma")
			{
				return true;
			}
		}
		// Check for special defines (that do not need a #define and that could
		// therefore appear in a sources file without any preprocessor
		// directive).
		if (line.find("__") != std::string::npos)
		{
			if (line.find("__DATE__") != std::string::npos ||
				line.find("__TIME__") != std::string::npos ||
				line.find("__FILE__") != std::string::npos ||
				line.find("__LINE__") != std::string::npos ||
				line.find("__AGENT_ID__") != std::string::npos ||
				line.find("__AGENT_NAME__") != std::string::npos ||
				line.find("__VIEWER_") != std::string::npos)
			{
				return true;
			}
		}
	}

	return false;
}
