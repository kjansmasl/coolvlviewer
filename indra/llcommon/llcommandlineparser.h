/**
 * @file llcommandlineparser.h
 * @brief LLCommandLineParser class declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLCOMMANDLINEPARSER_H
#define LL_LLCOMMANDLINEPARSER_H

#include "boost/function/function1.hpp"
#include "boost/program_options.hpp"

#include "llstring.h"

// LLCommandLineParser handles defining and parsing the command line.

class LLCommandLineParser
{
protected:
	LOG_CLASS(LLCommandLineParser);

public:
	typedef std::vector<std::string> token_vector_t;

	// Adds a value-less option to the command line description.
	// 'option_name' is the long name of the cmd-line option and 'description'
	// is the text description of the option usage.
	void addOptionDesc(const std::string& option_name,
					   boost::function1<void, const token_vector_t&> notify_callback = 0,
					   unsigned int num_tokens = 0,
					   const std::string& description = LLStringUtil::null,
					   const std::string& short_name = LLStringUtil::null,
					   bool composing = false, bool positional = false,
					   bool last_option = false);

	// Parses the command line given by argc/argv.
	bool parseCommandLine(int argc, char **argv);

	// Parses the command line contained by the given file.
	bool parseCommandLineString(const std::string& str);

	// Parses the command line contained by the given file.
	bool parseCommandLineFile(const std::basic_istream<char>& file);

	// Calls the callbacks associated with option descriptions; use this to
	// handle the results of parsing.
	void notify();

	// Prints a description of the configured options to the given ostream.
	// Useful for displaying usage info.
	std::ostream& printOptionsDesc(std::ostream& os) const;

	// Use these to retrieve get the values set for an option.

	bool hasOption(const std::string& name) const;
	// Returns an empty value if the option is not set.
	const token_vector_t& getOption(const std::string& name) const;

	// Prints the list of configured options.
	void printOptions() const;

	// Gets the error message, if it exists.
	LL_INLINE const std::string& getErrorMessage() const	{ return mErrorMsg; }

	// parser_func takes an input string, and should return a name/value pair
	// as the result.
	typedef boost::function1<std::pair<std::string,
									   std::string>, const std::string&> parser_func;

	// Adds a custom parser func to the parser.
	// Use this method to add a custom parser for parsing values that the
	// simple parser may not handle. It will be applied to each parameter
	// before the default parser gets a chance.
	LL_INLINE void setCustomParser(parser_func f)			{ mExtraParser = f; }

private:
	bool parseAndStoreResults(boost::program_options::command_line_parser& clp);

private:
	std::string mErrorMsg;
	parser_func mExtraParser;
};

LL_INLINE std::ostream& operator<<(std::ostream& out, const LLCommandLineParser& clp)
{
    return clp.printOptionsDesc(out);
}

#endif // LL_LLCOMMANDLINEPARSER_H
