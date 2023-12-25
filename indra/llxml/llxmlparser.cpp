/**
 * @file llxmlparser.cpp
 * @brief LLXmlParser implementation
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

#include "linden_common.h"

#include "llxmlparser.h"

LLXmlParser::LLXmlParser()
:	mParser(NULL),
	mDepth(0),
	mAuxErrorString("no error")
{
	// Override the document's declared encoding.
	mParser = XML_ParserCreate(NULL);

	XML_SetUserData(mParser, this);
	XML_SetElementHandler(mParser, startElementHandler, endElementHandler);
	XML_SetCharacterDataHandler(mParser, characterDataHandler);
	XML_SetProcessingInstructionHandler(mParser, processingInstructionHandler);
	XML_SetCommentHandler(mParser, commentHandler);
	XML_SetCdataSectionHandler(mParser, startCdataSectionHandler,
							   endCdataSectionHandler);

	// This sets the default handler but does not inhibit expansion of internal
	// entities. The entity reference will not be passed to the default
	// handler.
	XML_SetDefaultHandlerExpand(mParser, defaultDataHandler);

	XML_SetUnparsedEntityDeclHandler(mParser, unparsedEntityDeclHandler);
}

LLXmlParser::~LLXmlParser()
{
	if (mParser)
	{
		XML_ParserFree(mParser);
		mParser = NULL;
	}
}

bool LLXmlParser::parseFile(const std::string& path)
{
	llassert(!mDepth);

	bool success = true;

	LLFILE* file = LLFile::open(path, "rb");
	if (!file)
	{
		mAuxErrorString = llformat("Couldn't open file %s", path.c_str());
		success = false;
	}
	else
	{
		S32 bytes_read = 0;

		fseek(file, 0L, SEEK_END);
		S32 buffer_size = ftell(file);
		fseek(file, 0L, SEEK_SET);

		void* buffer = XML_GetBuffer(mParser, buffer_size);
		if (!buffer)
		{
			mAuxErrorString = llformat("Unable to allocate XML buffer while reading file %s",
									   path.c_str());
			success = false;
		}

		if (success)
		{
			bytes_read = (S32)fread(buffer, 1, buffer_size, file);
			if (bytes_read <= 0)
			{
				mAuxErrorString = llformat("Error while reading file %s",
										   path.c_str());
				success = false;
			}
		}

		if (success && !XML_ParseBuffer(mParser, bytes_read, true))
		{
			mAuxErrorString = llformat("Error while parsing file %s",
									   path.c_str());
			success = false;
		}

		LLFile::close(file);
	}

	if (success && mDepth)
	{
		llwarns << "mDepth not null after parsing: " << path << llendl;
		llassert(false);
	}
	mDepth = 0;

	if (!success)
	{
		llwarns << mAuxErrorString << llendl;
	}

	return success;
}

// Parses some input. Returns 0 if a fatal error is detected. The last call
// must have is_final true; len may be zero for this call (or any other).
S32 LLXmlParser::parse(const char* buf, int len, int is_final)
{
	return XML_Parse(mParser, buf, len, is_final);
}

const char* LLXmlParser::getErrorString()
{
	const char* error_string = XML_ErrorString(XML_GetErrorCode(mParser));
	if (!error_string)
	{
		error_string = mAuxErrorString.c_str();
	}
	return error_string;
}

S32 LLXmlParser::getCurrentLineNumber()
{
	return XML_GetCurrentLineNumber(mParser);
}

S32 LLXmlParser::getCurrentColumnNumber()
{
	return XML_GetCurrentColumnNumber(mParser);
}

///////////////////////////////////////////////////////////////////////////////
// Pseudo-private methods. These are only used by internal callbacks.

//static
void LLXmlParser::startElementHandler(void* user_data, const XML_Char* name,
									  const XML_Char** atts)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->startElement(name, atts);
		++self->mDepth;
	}
}

//static
void LLXmlParser::endElementHandler(void* user_data, const XML_Char* name)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		--self->mDepth;
		self->endElement(name);
	}
}

// s is not 0 terminated.
//static
void LLXmlParser::characterDataHandler(void* user_data, const XML_Char* s,
									   int len)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->characterData(s, len);
	}
}

// target and data are 0 terminated
//static
void LLXmlParser::processingInstructionHandler(void* user_data,
											   const XML_Char* target,
											   const XML_Char* data)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->processingInstruction(target, data);
	}
}

// data is 0 terminated
//static
void LLXmlParser::commentHandler(void* user_data, const XML_Char* data)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->comment(data);
	}
}

//static
void LLXmlParser::startCdataSectionHandler(void* user_data)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		++self->mDepth;
		self->startCdataSection();
	}
}

//static
void LLXmlParser::endCdataSectionHandler(void* user_data)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->endCdataSection();
		++self->mDepth;
	}
}

// This is called for any characters in the XML document for which there is no
// applicable handler. This includes both characters that are part of markup
// which is of a kind that is not reported (comments, markup declarations), or
// characters that are part of a construct which could be reported but for
// which no handler has been supplied. The characters are passed exactly as
// they were in the XML document except that they will be encoded in UTF-8.
// Line boundaries are not normalized. Note that a byte order mark character is
// not passed to the default handler. There are no guarantees about how
// characters are divided between calls to the default handler: for example, a
// comment might be split between multiple calls.
//static
void LLXmlParser::defaultDataHandler(void* user_data, const XML_Char* s,
									 int len)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->defaultData(s, len);
	}
}

// This is called for a declaration of an unparsed (NDATA) entity. The base
// argument is whatever was set by XML_SetBase. The entity_name, system_id and
// notation_name arguments will never be null. The other arguments may be.
//static
void LLXmlParser::unparsedEntityDeclHandler(void* user_data,
											const XML_Char* entity_name,
											const XML_Char* base,
											const XML_Char* system_id,
											const XML_Char* public_id,
											const XML_Char* notation_name)
{
	LLXmlParser* self = (LLXmlParser*)user_data;
	if (self)
	{
		self->unparsedEntityDecl(entity_name, base, system_id, public_id,
								 notation_name);
	}
}
