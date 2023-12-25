/** 
 * @file llmessagetemplateparser.cpp
 * @brief LLMessageTemplateParser implementation
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

#include "boost/tokenizer.hpp"

#include "llmessagetemplateparser.h"

// What follows is a bunch of C functions to do validation.

// Lets support a small subset of regular expressions here
// Syntax is a string made up of:
//	a	- checks against alphanumeric				([A-Za-z0-9])
//	c	- checks against character					([A-Za-z])
//	f	- checks against first variable character	([A-Za-z_])
//	v	- checks against variable					([A-Za-z0-9_])
//	s	- checks against sign of integer			([-0-9])
//  d	- checks against integer digit				([0-9])
//  *	- repeat last check

// Checks 'a'
bool b_return_alphanumeric_ok(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		   (c >= '0' && c <= '9');
}

// Checks 'c'
bool b_return_character_ok(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Checks 'f'
bool b_return_first_variable_ok(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

// Checks 'v'
bool b_return_variable_ok(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		   (c >= '0' && c <= '9') || c == '_';
}

// Checks 's'
bool b_return_signed_integer_ok(char c)
{
	return (c >= '0' && c <= '9') || c == '-';
}

// Checks 'd'
bool b_return_integer_ok(char c)
{
	return c >= '0' && c <= '9';
}

bool (*gParseCheckCharacters[])(char c) =
{
	b_return_alphanumeric_ok,
	b_return_character_ok,
	b_return_first_variable_ok,
	b_return_variable_ok,
	b_return_signed_integer_ok,
	b_return_integer_ok
};

S32 get_checker_number(char checker)
{
	switch (checker)
	{
		case 'a':
			return 0;

		case 'c':
			return 1;

		case 'f':
			return 2;

		case 'v':
			return 3;

		case 's':
			return 4;

		case 'd':
			return 5;

		case '*':
			return 9999;

		default:
			return -1;
	}
}

// Checks token based on passed simplified regular expression
bool b_check_token(const char* token, const char* regexp)
{
	S32 tptr, rptr = 0;
	S32 current_checker, next_checker = 0;

	current_checker = get_checker_number(regexp[rptr++]);

	if (current_checker == -1)
	{
		llerrs << "Invalid regular expression value!" << llendl;
		return false;
	}

	if (current_checker == 9999)
	{
		llerrs << "Regular expression can't start with *!" << llendl;
		return false;
	}

	for (tptr = 0; token[tptr]; ++tptr)
	{
		if (current_checker == -1)
		{
			llerrs << "Input exceeds regular expression!\nDid you forget a *?"
				   << llendl;
			return false;
		}

		if (!gParseCheckCharacters[current_checker](token[tptr]))
		{
			return false;
		}
		if (next_checker != 9999)
		{
			next_checker = get_checker_number(regexp[rptr++]);
			if (next_checker != 9999)
			{
				current_checker = next_checker;
			}
		}
	}
	return true;
}

// C variable can be made up of upper or lower case letters, underscores, or
// numbers, but can't start with a number
bool b_variable_ok(const char* token)
{
	if (b_check_token(token, "fv*"))
	{
		return true;
	}

	llwarns << "Token '" << token << "' is not a variable !" << llendl;
	return false;
}

// An integer is made up of the digits 0-9 and may be preceded by a '-'
bool b_integer_ok(const char* token)
{
	if (b_check_token(token, "sd*"))
	{
		return true;
	}

	llwarns << "Token is not an integer !" << llendl;
	return false;
}

// An integer is made up of the digits 0-9
bool b_positive_integer_ok(const char* token)
{
	if (b_check_token(token, "d*"))
	{
		return true;
	}

	llwarns << "Token is not an integer !" << llendl;
	return false;
}

// Done with C functions, here's the tokenizer.

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;	

LLTemplateTokenizer::LLTemplateTokenizer(const std::string& contents)
:	mStarted(false),
	mTokens()
{
	boost::char_separator<char> newline("\r\n", "", boost::keep_empty_tokens);
	boost::char_separator<char> spaces(" \t");
	U32 line_counter = 1;
	
	tokenizer line_tokens(contents, newline);
	for (tokenizer::iterator line_iter = line_tokens.begin();
		 line_iter != line_tokens.end(); ++line_iter, ++line_counter)
	{
		tokenizer word_tokens(*line_iter, spaces);
		for (tokenizer::iterator word_iter = word_tokens.begin();
			 word_iter != word_tokens.end(); ++word_iter)
		{
			if ((*word_iter)[0] == '/')
			{
				break;   // skip to end of line on comments
			}
			positioned_token pt;// = new positioned_token();
			pt.str = std::string(*word_iter);
			pt.line = line_counter;
			mTokens.push_back(pt);
		}
	}
	mCurrent = mTokens.begin();
}

void LLTemplateTokenizer::inc()
{
	if (atEOF())
	{
		error("trying to increment token of EOF");
	}
	else if (mStarted)
	{
		++mCurrent;
	}
	else
	{
		mStarted = true;
		mCurrent = mTokens.begin();
	}
}

void LLTemplateTokenizer::dec()
{
	if (mCurrent == mTokens.begin())
	{
		if (mStarted)
		{
			mStarted = false;
		}
		else
		{
			error("trying to decrement past beginning of file");
		}
	}
	else
	{
		--mCurrent;
	}
}

std::string LLTemplateTokenizer::get() const
{
	if (atEOF())
	{
		error("trying to get EOF");
	}
	return mCurrent->str;
}

U32 LLTemplateTokenizer::line() const
{
	return atEOF() ? 0 : mCurrent->line;
}

bool LLTemplateTokenizer::atEOF() const
{
	return mCurrent == mTokens.end();
}

std::string LLTemplateTokenizer::next()
{
	inc();
	return get();
}

bool LLTemplateTokenizer::want(const std::string& token)
{
	if (atEOF())
	{
		return false;
	}
	inc();
	if (atEOF())
	{
		return false;
	}
	if (get() != token)
	{
		dec(); // back up a step
		return false;
	}
	return true;
}

bool LLTemplateTokenizer::wantEOF()
{
	// See if the next token is EOF
	if (atEOF())
	{
		return true;
	}
	inc();
	if (!atEOF())
	{
		dec();			// Back up a step
		return false;
	}
	return true;
}

void LLTemplateTokenizer::error(std::string message) const
{
	if (atEOF())
	{
		llerrs << "Unexpected end of file: " << message << llendl;
	}
	else
	{
		llerrs << "Problem parsing message template at line " << line()
			   << ", with token '" << get() << "' : " << message << llendl;
	}
}

// Done with tokenizer, next is the parser.

LLTemplateParser::LLTemplateParser(LLTemplateTokenizer& tokens)
:	mVersion(0.f),
	mMessages()
{
	// The version number should be the first thing in the file
	if (tokens.want("version"))
	{
		// version number
		std::string vers_string = tokens.next();
		mVersion = (F32)atof(vers_string.c_str());
		
		llinfos << "### Message template version " << mVersion << "  ###"
				<< llendl;
	}
	else
	{
		llerrs << "Version must be first in the message template, found "
			   << tokens.next() << llendl;
	}

	while (LLMessageTemplate* templatep = parseMessage(tokens))
	{
		if (templatep->getDeprecation() != MD_DEPRECATED)
		{
			mMessages.push_back(templatep);
		}
		else
		{
			delete templatep;
		}
	}

	if (!tokens.wantEOF())
	{
		llerrs << "Expected end of template or a message, instead found: "
			   << tokens.next() << " at " << tokens.line() << llendl;
	}
}

F32 LLTemplateParser::getVersion() const
{
	return mVersion;
}

LLTemplateParser::message_iterator LLTemplateParser::getMessagesBegin() const
{
	return mMessages.begin();
}

LLTemplateParser::message_iterator LLTemplateParser::getMessagesEnd() const
{
	return mMessages.end();
}

// static
LLMessageTemplate* LLTemplateParser::parseMessage(LLTemplateTokenizer& tokens)
{
	if (!tokens.want("{"))
	{
		return NULL;
	}

	// Name first
	std::string template_name = tokens.next();	
	// Is name a legit C variable name ?
	if (!b_variable_ok(template_name.c_str()))
	{
		llerrs << "Not legit variable name: " << template_name << " at "
			   << tokens.line() << llendl;
	}

	LLMessageTemplate* templatep = NULL;
	// OK, now get Frequency ("High", "Medium", or "Low")
	EMsgFrequency frequency = MFT_LOW;
	std::string freq_string = tokens.next();
	if (freq_string == "High")
	{
		frequency = MFT_HIGH;
	}
	else if (freq_string == "Medium")
	{
		frequency = MFT_MEDIUM;
	}
	else if (freq_string == "Low" || freq_string == "Fixed")
	{
		frequency = MFT_LOW;
	}
	else
	{
		llerrs << "Expected frequency, got " << freq_string << " at "
			   << tokens.line() << llendl;
	}

	// *TODO: more explicit checking here please
	U32 message_number = strtoul(tokens.next().c_str(), NULL, 0);

	switch (frequency)
	{
		case MFT_HIGH:
			break;

		case MFT_MEDIUM:
			message_number = (255 << 8) | message_number;
			break;

		case MFT_LOW:
			message_number = (255 << 24) | (255 << 16) | message_number;
			break;

		default:
			llerrs << "Unknown frequency enum: " << frequency << llendl;
	}
   
	templatep = new LLMessageTemplate(template_name.c_str(), message_number,
									  frequency);
		
	// Now get trust ("Trusted", "NotTrusted")
	std::string trust = tokens.next();
	if (trust == "Trusted")
	{
		templatep->setTrust(MT_TRUST);
	}
	else if (trust == "NotTrusted")
	{
		templatep->setTrust(MT_NOTRUST);
	}
	else
	{
		llerrs << "Bad trust " << trust << " at " << tokens.line() << llendl;
	}
	
	// Get encoding
	std::string encoding = tokens.next();
	if (encoding == "Unencoded")
	{
		templatep->setEncoding(ME_UNENCODED);
	}
	else if (encoding == "Zerocoded")
	{
		templatep->setEncoding(ME_ZEROCODED);
	}
	else
	{
		llerrs << "Bad encoding " << encoding << " at " << tokens.line()
			   << llendl;
	}

	// Get deprecation
	if (tokens.want("Deprecated"))
	{
		templatep->setDeprecation(MD_DEPRECATED);
	}
	else if (tokens.want("UDPDeprecated"))
	{
		templatep->setDeprecation(MD_UDPDEPRECATED);
	}
	else if (tokens.want("UDPBlackListed"))
	{
		templatep->setDeprecation(MD_UDPBLACKLISTED);
	}
	else if (tokens.want("NotDeprecated"))
	{
		// This is the default value, but it can't hurt to set it twice
		templatep->setDeprecation(MD_NOTDEPRECATED);
	}
	// else ... it is probably a brace, let's just start block processing

	while (LLMessageBlock * blockp = parseBlock(tokens))
	{
		templatep->addBlock(blockp);
	}
	
	if (!tokens.want("}"))
	{
		llerrs << "Expecting closing } for message " << template_name << " at "
			   << tokens.line() << llendl;
	}
	return templatep;
}

// static
LLMessageBlock* LLTemplateParser::parseBlock(LLTemplateTokenizer& tokens)
{
	if (!tokens.want("{"))
	{
		return NULL;
	}

	// Name first
	std::string block_name = tokens.next();
	// Is name a legit C variable name ? 
	if (!b_variable_ok(block_name.c_str()))
	{
		llerrs << "not a legal block name: " << block_name << " at "
			   << tokens.line() << llendl;
	}

	LLMessageBlock* blockp = NULL;
	// now, block type ("Single", "Multiple", or "Variable")
	std::string block_type = tokens.next();
	// which one is it?
	if (block_type == "Single")
	{
		// OK, we can create a block
		blockp = new LLMessageBlock(block_name.c_str(), MBT_SINGLE);
	}
	else if (block_type == "Multiple")
	{
		// Need to get the number of repeats
		std::string repeats = tokens.next();
		
		// Is it a legal integer ?
		if (!b_positive_integer_ok(repeats.c_str()))
		{
			llerrs << "not a legal integer for block multiple count: "
				   << repeats << " at " << tokens.line() << llendl;
		}
		
		// OK, we can create a block
		blockp = new LLMessageBlock(block_name.c_str(), MBT_MULTIPLE,
									atoi(repeats.c_str()));
	}
	else if (block_type == "Variable")
	{
		// OK, we can create a block
		blockp = new LLMessageBlock(block_name.c_str(), MBT_VARIABLE);
	}
	else
	{
		llerrs << "bad block type: " << block_type
			   << " at " << tokens.line() << llendl;
	}

	while (LLMessageVariable* varp = parseVariable(tokens))
	{
		blockp->addVariable(varp->getName(), varp->getType(), varp->getSize());
		delete varp;
	}

	if (!tokens.want("}"))
	{
		llerrs << "Expecting closing } for block " << block_name << " at "
			   << tokens.line() << llendl;
	}
	return blockp;
}

// static
LLMessageVariable* LLTemplateParser::parseVariable(LLTemplateTokenizer& tokens)
{
	if (!tokens.want("{"))
	{
		return NULL;
	}

	std::string var_name = tokens.next();
	if (!b_variable_ok(var_name.c_str()))
	{
		llerrs << "Not a legit variable name: " << var_name << " at "
			   << tokens.line() << llendl;
	}

	LLMessageVariable* varp = NULL;
	std::string var_type = tokens.next();

	if (var_type == "U8")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_U8, 1);					
	}
	else if (var_type == "U16")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_U16, 2);					
	}
	else if (var_type == "U32")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_U32, 4);					
	}
	else if (var_type == "U64")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_U64, 8);					
	}
	else if (var_type == "S8")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_S8, 1);					
	}
	else if (var_type == "S16")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_S16, 2);					
	}
	else if (var_type == "S32")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_S32, 4);					
	}
	else if (var_type == "S64")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_S64, 8);					
	}
	else if (var_type == "F32")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_F32, 4);					
	}
	else if (var_type == "F64")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_F64, 8);					
	}
	else if (var_type == "LLVector3")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_LLVector3, 12);					
	}
	else if (var_type == "LLVector3d")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_LLVector3d, 24);
	}
	else if (var_type == "LLVector4")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_LLVector4, 16);					
	}
	else if (var_type == "LLQuaternion")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_LLQuaternion, 12);
	}
	else if (var_type == "LLUUID")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_LLUUID, 16);					
	}
	else if (var_type == "BOOL")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_BOOL, 1);					
	}
	else if (var_type == "IPADDR")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_IP_ADDR, 4);					
	}
	else if (var_type == "IPPORT")
	{
		varp = new LLMessageVariable(var_name.c_str(), MVT_IP_PORT, 2);
	}
	else if (var_type == "Fixed" || var_type == "Variable")
	{
		std::string variable_size = tokens.next();
		
		if (!b_positive_integer_ok(variable_size.c_str()))
		{
			llerrs << "not a legal integer variable size: " << variable_size
				   << " at " << tokens.line() << llendl;
		}

		EMsgVariableType type_enum;
		if (var_type == "Variable")
		{
			type_enum = MVT_VARIABLE;
		}
		else if (var_type == "Fixed")
		{
			type_enum = MVT_FIXED;
		}
		else
		{
			type_enum = MVT_FIXED; // removes a warning
			llerrs << "bad variable type: " << var_type << " at "
				   << tokens.line() << llendl;
		}

		varp = new LLMessageVariable(var_name.c_str(), type_enum,
									 atoi(variable_size.c_str()));
	}
	else
	{
		llerrs << "bad variable type:" << var_type << " at " << tokens.line()
			   << llendl;
	}

	if (!tokens.want("}"))
	{
		llerrs << "Expecting closing } for variable " << var_name << " at "
			   << tokens.line() << llendl;
	}

	return varp;
}
