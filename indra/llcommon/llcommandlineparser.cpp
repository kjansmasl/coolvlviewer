/**
 * @file llcommandlineparser.cpp
 * @brief The LLCommandLineParser class definitions
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

#include "linden_common.h"

#include <iostream>
#include <sstream>

#include "boost/tokenizer.hpp"

#include "llcommandlineparser.h"

namespace po = boost::program_options;

// Note: currently the boost object reside in file scope. This has a couple of
// negative impacts: they are always around and there can be only one instance
// of each. The plus is that the boost-ly-ness of this implementation is hidden
// from the rest of the world. It is important to realize that multiple
// LLCommandLineParser objects will all have this single repository of option
// descriptions and parsed options. This could be good or bad, and probably
// would not matter for most use cases.
namespace
{
	po::options_description gOptionsDesc;
	po::positional_options_description gPositionalOptions;
	po::variables_map gVariableMap;

	const LLCommandLineParser::token_vector_t gEmptyValue;

	void read_file_into_string(std::string& str,
							   const std::basic_istream<char>& file)
	{
		std::ostringstream oss;
		oss << file.rdbuf();
		str = oss.str();
	}

	bool gPastLastOption = false;
}

class LLCLPError : public std::logic_error
{
public:
	LLCLPError(const std::string& what)
	:	std::logic_error(what)
	{
	}
};

class LLCLPLastOption : public std::logic_error
{
public:
	LLCLPLastOption(const std::string& what)
	:	std::logic_error(what)
	{
	}
};

class LLCLPValue : public po::value_semantic_codecvt_helper<char>
{
public:
	LLCLPValue()
	:	mMinTokens(0),
		mMaxTokens(0),
		mIsComposing(false),
		mLastOption(false)
	{
	}

	virtual ~LLCLPValue()
	{
	}

	LL_INLINE void setMinTokens(unsigned c)
	{
		mMinTokens = c;
	}

	LL_INLINE void setMaxTokens(unsigned c)
	{
		mMaxTokens = c;
	}

	LL_INLINE void setComposing(bool c)
	{
		mIsComposing = c;
	}

	LL_INLINE void setLastOption(bool c)
	{
		mLastOption = c;
	}

	typedef boost::function1<void,
							 const LLCommandLineParser::token_vector_t&> notify_callback_t;

	LL_INLINE void setNotifyCallback(notify_callback_t f)
	{
		mNotifyCallback = f;
	}

	// Overrides to support the value_semantic interface.
	virtual std::string name() const
	{
		const std::string arg("arg");
		const std::string args("args");
		return max_tokens() > 1 ? args : arg;
	}

	virtual unsigned min_tokens() const
	{
		return mMinTokens;
	}

	virtual unsigned max_tokens() const
	{
		return mMaxTokens;
	}

	virtual bool is_composing() const
	{
		return mIsComposing;
	}

	// Needed for boost >= 1.42
	virtual bool is_required() const
	{
		return false; // All our command line options are optional.
	}

	virtual bool apply_default(boost::any& value_store) const
	{
		return false; // No defaults.
	}

	// Needed for boost >= 1.59
	virtual bool adjacent_tokens_only() const
	{
		return false;
	}

	virtual void notify(const boost::any& value_store) const
	{
		const LLCommandLineParser::token_vector_t* value =
			boost::any_cast<const LLCommandLineParser::token_vector_t>(&value_store);
		if (mNotifyCallback)
		{
		   mNotifyCallback(*value);
		}

	}

protected:
	void xparse(boost::any& value_store,
				const std::vector<std::string>& new_tokens) const
	{
		if (gPastLastOption)
		{
			throw(LLCLPLastOption("Don't parse no more!"));
		}

		// Error checks. Needed?
		if (!value_store.empty() && !is_composing())
		{
			throw(LLCLPError("Non composing value with multiple occurences."));
		}
		if (new_tokens.size() < min_tokens() || new_tokens.size() > max_tokens())
		{
			throw(LLCLPError("Illegal number of tokens specified."));
		}

		if (value_store.empty())
		{
			value_store = boost::any(LLCommandLineParser::token_vector_t());
		}
		LLCommandLineParser::token_vector_t* tv =
			boost::any_cast<LLCommandLineParser::token_vector_t>(&value_store);

		for (unsigned i = 0; i < new_tokens.size() && i < mMaxTokens; ++i)
		{
			tv->push_back(new_tokens[i]);
		}

		if (mLastOption)
		{
			gPastLastOption = true;
		}
	}

protected:
	unsigned			mMinTokens;
	unsigned			mMaxTokens;
	notify_callback_t	mNotifyCallback;
	bool				mIsComposing;
	bool				mLastOption;
};

//----------------------------------------------------------------------------
// LLCommandLineParser defintions
//----------------------------------------------------------------------------
void LLCommandLineParser::addOptionDesc(const std::string& option_name,
										boost::function1<void, const token_vector_t&> notify_callback,
										unsigned int token_count,
										const std::string& description,
										const std::string& short_name,
										bool composing,
										bool positional,
										bool last_option)
{
	// Compose the name for boost::po.
	// It takes the format "long_name, short name"
	const std::string comma(",");
	std::string boost_option_name = option_name;
	if (short_name != LLStringUtil::null)
	{
		boost_option_name += comma;
		boost_option_name += short_name;
	}

	LLCLPValue* value_desc = new LLCLPValue();
	value_desc->setMinTokens(token_count);
	value_desc->setMaxTokens(token_count);
	value_desc->setComposing(composing);
	value_desc->setLastOption(last_option);

	std::string desc = " : " + description;
	// Note: we cannot replace boost::shared_ptr with std::shared_ptr here,
	// because boost::program_options lacks the templates for it... HB
	boost::shared_ptr<po::option_description> d(new po::option_description(boost_option_name.c_str(),
												value_desc, desc.c_str()));

	if (!notify_callback.empty())
	{
		value_desc->setNotifyCallback(notify_callback);
	}

	gOptionsDesc.add(d);

	if (positional)
	{
		gPositionalOptions.add(boost_option_name.c_str(), token_count);
	}
}

bool LLCommandLineParser::parseAndStoreResults(po::command_line_parser& clp)
{
	try
	{
		clp.options(gOptionsDesc);
		clp.positional(gPositionalOptions);

		// SNOW-626: Boost 1.42 erroneously added allow_guessing to the default
		// style (see:
		// http://groups.google.com/group/boost-list/browse_thread/thread/545d7bf98ff9bb16?fwc=2&pli=1)
		// Remove allow_guessing from the default style, because that is not
		// allowed when we have options that are a prefix of other options
		// (aka, --help and --helperuri).
		clp.style((po::command_line_style::default_style &
				   ~po::command_line_style::allow_guessing) |
				  po::command_line_style::allow_long_disguise);

		if (mExtraParser)
		{
			clp.extra_parser(mExtraParser);
		}

		po::basic_parsed_options<char> opts = clp.run();
		po::store(opts, gVariableMap);
	}
	catch (po::error& e)
	{
		llwarns << "Caught Error:" << e.what() << llendl;
		mErrorMsg = e.what();
		return false;
	}
	catch (LLCLPError& e)
	{
		llwarns << "Caught Error:" << e.what() << llendl;
		mErrorMsg = e.what();
		return false;
	}
	catch (LLCLPLastOption&)
	{
		// This exception means a token was read after an option that must be
		// the last option was reached (see url and slurl options)

		// boost::po will have stored a malformed option. All such options will
		// be removed below. The last option read, the last_option option, and
		// its value are put into the error message.
		std::string last_option;
		std::string last_value;
		for (po::variables_map::iterator i = gVariableMap.begin();
			 i != gVariableMap.end(); )
		{
			po::variables_map::iterator tempI = i++;
			if (tempI->second.empty())
			{
				gVariableMap.erase(tempI);
			}
			else
			{
				last_option = tempI->first;
				LLCommandLineParser::token_vector_t* tv;
				tv = boost::any_cast<LLCommandLineParser::token_vector_t>(&(tempI->second.value()));
				if (tv && !tv->empty())
				{
					last_value = (*tv)[tv->size() - 1];
				}
			}
		}

		// Continue without parsing.
		std::ostringstream msg;
		msg << "Caught Error: Found options after last option: " << last_option
			<< " " << last_value;

		llwarns << msg.str() << llendl;
		mErrorMsg = msg.str();
		return false;
	}
	return true;
}

bool LLCommandLineParser::parseCommandLine(int argc, char** argv)
{
	po::command_line_parser clp(argc, argv);
	return parseAndStoreResults(clp);
}

bool LLCommandLineParser::parseCommandLineString(const std::string& str)
{
	std::string cmd_line;
	if (!str.empty())
	{
		bool add_last_c = true;
		U32 last_c_pos = str.size() - 1;
		for (U32 pos = 0; pos < last_c_pos; ++pos)
		{
			cmd_line.append(&str[pos], 1);
			if (str[pos] == '\\')
			{
				cmd_line += "\\";
				if (str[pos + 1] == '\\')
				{
					add_last_c = pos++ != last_c_pos;
				}
			}
		}
		if (add_last_c)
		{
			cmd_line.append(&str[last_c_pos], 1);
			if (str[last_c_pos] == '\\')
			{
				cmd_line += "\\";
			}
		}
	}

	// Split the string content into tokens
	boost::escaped_list_separator<char> sep("\\", "\r\n ", "\"'");
	boost::tokenizer< boost::escaped_list_separator<char> > tok(cmd_line, sep);
	std::vector<std::string> tokens;
	// std::copy(tok.begin(), tok.end(), std::back_inserter(tokens));
	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator
			i = tok.begin();
		 i != tok.end(); ++i)
	{
		if (i->size())
		{
			tokens.push_back(*i);
		}
	}

	po::command_line_parser clp(tokens);
	return parseAndStoreResults(clp);
}

bool LLCommandLineParser::parseCommandLineFile(const std::basic_istream<char>& file)
{
	std::string args;
	read_file_into_string(args, file);

	return parseCommandLineString(args);
}

void LLCommandLineParser::notify()
{
	po::notify(gVariableMap);
}

void LLCommandLineParser::printOptions() const
{
	for (po::variables_map::iterator i = gVariableMap.begin();
		 i != gVariableMap.end(); ++i)
	{
		std::string name = i->first;
		token_vector_t values = i->second.as<token_vector_t>();
		std::ostringstream oss;
		oss << name << ": ";
		for (token_vector_t::iterator t_itr = values.begin();
			 t_itr != values.end(); ++t_itr)
		{
			oss << t_itr->c_str() << " ";
		}
		llinfos << oss.str() << llendl;
	}
}

std::ostream& LLCommandLineParser::printOptionsDesc(std::ostream& os) const
{
	return os << gOptionsDesc;
}

bool LLCommandLineParser::hasOption(const std::string& name) const
{
	return gVariableMap.count(name) > 0;
}

const LLCommandLineParser::token_vector_t& LLCommandLineParser::getOption(const std::string& name) const
{
	if (hasOption(name))
	{
		return gVariableMap[name].as<token_vector_t>();
	}

	return gEmptyValue;
}
