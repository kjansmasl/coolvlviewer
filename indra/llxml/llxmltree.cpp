/**
 * @file llxmltree.cpp
 * @brief LLXmlTree implementation
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

#include "llxmltree.h"

#include "llcolor3.h"
#include "llcolor4.h"
#include "llcolor4u.h"
#include "llquaternion.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llvector4.h"

///////////////////////////////////////////////////////////////////////////////
// LLXmlTree class
///////////////////////////////////////////////////////////////////////////////

//static
LLStdStringTable LLXmlTree::sAttributeKeys(1024);

LLXmlTree::LLXmlTree()
:	mRoot(NULL),
	mNodeNames(512)
{
}

LLXmlTree::~LLXmlTree()
{
	cleanup();
}

void LLXmlTree::cleanup()
{
	delete mRoot;
	mRoot = NULL;
	mNodeNames.cleanup();
}

bool LLXmlTree::parseFile(const std::string& path, bool keep_contents)
{
	delete mRoot;
	mRoot = NULL;

	LLXmlTreeParser parser(this);
	bool success = parser.parseFile(path, &mRoot, keep_contents);
	if (!success)
	{
		S32 line_number = parser.getCurrentLineNumber();
		const char* error =  parser.getErrorString();
		llwarns << "Parse file failed line " << line_number << " with error: "
				<< error << llendl;
	}
	return success;
}

void LLXmlTree::dump()
{
	if (mRoot)
	{
		dumpNode(mRoot, "    ");
	}
}

void LLXmlTree::dumpNode(LLXmlTreeNode* node, const std::string& prefix)
{
	node->dump(prefix);

	std::string new_prefix = prefix + "    ";
	for (LLXmlTreeNode* child = node->getFirstChild(); child;
		 child = node->getNextChild())
	{
		dumpNode(child, new_prefix);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLXmlTreeNode class
///////////////////////////////////////////////////////////////////////////////

LLXmlTreeNode::LLXmlTreeNode(const char* name, LLXmlTreeNode* parent,
							 LLXmlTree* tree)
:	mName(name),
	mParent(parent),
	mTree(tree)
{
}

LLXmlTreeNode::~LLXmlTreeNode()
{
	for (attribute_map_t::iterator it = mAttributes.begin(),
								   end = mAttributes.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		delete mChildren[i];
	}
	mChildren.clear();
}

void LLXmlTreeNode::dump(const std::string& prefix)
{
	llinfos << prefix << mName;
	if (!mContents.empty())
	{
		llcont << " contents = \"" << mContents << "\"";
	}
	attribute_map_t::iterator iter;
	for (iter = mAttributes.begin(); iter != mAttributes.end(); ++iter)
	{
		LLStdStringHandle key = iter->first;
		const std::string* value = iter->second;
		llcont << prefix << " " << key << "="
			   << (value->empty() ? "NULL" : *value);
	}
	llcont << llendl;
}

bool LLXmlTreeNode::hasAttribute(const std::string& name)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	attribute_map_t::iterator iter = mAttributes.find(canonical_name);
	return iter != mAttributes.end();
}

void LLXmlTreeNode::addAttribute(const char* name, const std::string& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	const std::string* newstr = new std::string(value);
	mAttributes[canonical_name] = newstr; // insert + copy
}

LLXmlTreeNode*	LLXmlTreeNode::getFirstChild()
{
	mChildrenIter = mChildren.begin();
	return getNextChild();
}

LLXmlTreeNode* LLXmlTreeNode::getNextChild()
{
	return mChildrenIter == mChildren.end() ? 0 : *mChildrenIter++;
}

LLXmlTreeNode* LLXmlTreeNode::getChildByName(const std::string& name)
{
	LLStdStringHandle tableptr = mTree->mNodeNames.checkString(name);
	mChildMapIter = mChildMap.lower_bound(tableptr);
	mChildMapEndIter = mChildMap.upper_bound(tableptr);
	return getNextNamedChild();
}

LLXmlTreeNode* LLXmlTreeNode::getNextNamedChild()
{
	if (mChildMapIter == mChildMapEndIter)
	{
		return NULL;
	}
	else
	{
		return (mChildMapIter++)->second;
	}
}

void LLXmlTreeNode::appendContents(const std::string& str)
{
	mContents.append(str);
}

void LLXmlTreeNode::addChild(LLXmlTreeNode* child)
{
	llassert(child);
	mChildren.push_back(child);

	// Add a name mapping to this node
	LLStdStringHandle tableptr = mTree->mNodeNames.insert(child->mName);
	mChildMap.emplace(tableptr, child);

	child->mParent = this;
}

///////////////////////////////////////////////////////////////////////////////
// These functions assume that name is already in mAttritrubteKeys

bool LLXmlTreeNode::getFastAttributeBool(LLStdStringHandle canonical_name,
										 bool& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToBool(*s, value);
}

bool LLXmlTreeNode::getFastAttributeU8(LLStdStringHandle canonical_name,
									   U8& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToU8(*s, value);
}

bool LLXmlTreeNode::getFastAttributeS8(LLStdStringHandle canonical_name,
									   S8& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToS8(*s, value);
}

bool LLXmlTreeNode::getFastAttributeS16(LLStdStringHandle canonical_name,
										S16& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToS16(*s, value);
}

bool LLXmlTreeNode::getFastAttributeU16(LLStdStringHandle canonical_name,
										U16& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToU16(*s, value);
}

bool LLXmlTreeNode::getFastAttributeU32(LLStdStringHandle canonical_name,
										U32& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToU32(*s, value);
}

bool LLXmlTreeNode::getFastAttributeS32(LLStdStringHandle canonical_name,
										S32& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToS32(*s, value);
}

bool LLXmlTreeNode::getFastAttributeF32(LLStdStringHandle canonical_name,
										F32& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToF32(*s, value);
}

bool LLXmlTreeNode::getFastAttributeF64(LLStdStringHandle canonical_name,
										F64& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLStringUtil::convertToF64(*s, value);
}

bool LLXmlTreeNode::getFastAttributeColor(LLStdStringHandle canonical_name,
										  LLColor4& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLColor4::parseColor(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeColor4(LLStdStringHandle canonical_name,
										   LLColor4& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLColor4::parseColor4(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeColor4U(LLStdStringHandle canonical_name,
											LLColor4U& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLColor4U::parseColor4U(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeVector3(LLStdStringHandle canonical_name,
											LLVector3& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLVector3::parseVector3(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeVector3d(LLStdStringHandle canonical_name,
											 LLVector3d& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLVector3d::parseVector3d(*s,  &value);
}

bool LLXmlTreeNode::getFastAttributeQuat(LLStdStringHandle canonical_name,
										 LLQuaternion& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLQuaternion::parseQuat(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeUUID(LLStdStringHandle canonical_name,
										 LLUUID& value)
{
	const std::string* s = getAttribute(canonical_name);
	return s && LLUUID::parseUUID(*s, &value);
}

bool LLXmlTreeNode::getFastAttributeString(LLStdStringHandle canonical_name,
										   std::string& value)
{
	const std::string* s = getAttribute(canonical_name);
	if (s)
	{
		value = *s;
		return true;
	}
	else
	{
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool LLXmlTreeNode::getAttributeBool(const char* name, bool& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeBool(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeU8(const char* name, U8& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeU8(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeS8(const char* name, S8& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeS8(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeS16(const char* name, S16& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeS16(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeU16(const char* name, U16& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeU16(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeU32(const char* name, U32& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeU32(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeS32(const char* name, S32& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeS32(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeF32(const char* name, F32& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeF32(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeF64(const char* name, F64& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeF64(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeColor(const char* name, LLColor4& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeColor(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeColor4(const char* name, LLColor4& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeColor4(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeColor4U(const char* name, LLColor4U& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeColor4U(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeVector3(const char* name, LLVector3& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeVector3(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeVector3d(const char* name, LLVector3d& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeVector3d(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeQuat(const char* name, LLQuaternion& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeQuat(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeUUID(const char* name, LLUUID& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeUUID(canonical_name, value);
}

bool LLXmlTreeNode::getAttributeString(const char* name, std::string& value)
{
	LLStdStringHandle canonical_name = LLXmlTree::sAttributeKeys.addString(name);
	return getFastAttributeString(canonical_name, value);
}

// The following xml <message> nodes will all return the string from
// getTextContents():
//  "The quick brown fox\n  Jumps over the lazy dog"
//
//  1. HTML paragraph format:
//		<message>
//		<p>The quick brown fox</p>
//		<p>  Jumps over the lazy dog</p>
//		</message>
//  2. Each quoted section -> paragraph:
//		<message>
//		"The quick brown fox"
//		"  Jumps over the lazy dog"
//		</message>
//  3. Literal text with beginning and trailing whitespace removed:
//		<message>
// The quick brown fox
//   Jumps over the lazy dog
//		</message>

std::string LLXmlTreeNode::getTextContents()
{
	std::string msg;
	LLXmlTreeNode* p = getChildByName("p");
	if (p)
	{
		// Case 1: node has <p>text</p> tags
		while (p)
		{
			msg += p->getContents() + "\n";
			p = getNextNamedChild();
		}
	}
	else
	{
		std::string::size_type n = mContents.find_first_not_of(" \t\n");
		if (n != std::string::npos && mContents[n] == '\"')
		{
			// Case 2: node has quoted text
			S32 num_lines = 0;
			while (true)
			{
				// mContents[n] == '"'
				std::string::size_type t = ++n;
				std::string::size_type m = 0;
				// fix-up escaped characters
				while (true)
				{
					// Find first of \ or "
					m = mContents.find_first_of("\\\"", t);
					if (m == std::string::npos || mContents[m] == '\"')
					{
						break;
					}
					mContents.erase(m, 1);
					t = m + 1;
				}
				if (m == std::string::npos)
				{
					break;
				}
				// mContents[m] == '"'
				++num_lines;
				msg += mContents.substr(n, m - n) + "\n";
				n = mContents.find_first_of("\"", m + 1);
				if (n == std::string::npos)
				{
					if (num_lines == 1)
					{
						// Remove "\n" if only 1 line
						msg.erase(msg.size() - 1);
					}
					break;
				}
			}
		}
		else
		{
			// Case 3: node has embedded text (beginning and trailing
			// whitespace trimmed)
			msg = mContents;
		}
	}
	return msg;
}

///////////////////////////////////////////////////////////////////////////////
// LLXmlTreeParser class
///////////////////////////////////////////////////////////////////////////////

LLXmlTreeParser::LLXmlTreeParser(LLXmlTree* tree)
:	mTree(tree),
	mRoot(NULL),
	mCurrent(NULL),
	mDump(false),
	mKeepContents(false)
{
}

bool LLXmlTreeParser::parseFile(const std::string& path, LLXmlTreeNode** root,
								bool keep_contents)
{
	llassert(!mRoot);
	llassert(!mCurrent);

	mKeepContents = keep_contents;

	bool success = LLXmlParser::parseFile(path);

	*root = mRoot;
	mRoot = NULL;

	if (success && mCurrent)
	{
		llwarns << "mCurrent not null after parsing: " << path << llendl;
		llassert(false);
	}
	mCurrent = NULL;

	return success;
}

const std::string& LLXmlTreeParser::tabs()
{
	static std::string s;
	s.clear();
	S32 num_tabs = getDepth() - 1;
	for (S32 i = 0; i < num_tabs; ++i)
	{
		s += "    ";
	}
	return s;
}

void LLXmlTreeParser::startElement(const char* name, const char** atts)
{
	if (mDump)
	{
		llinfos << tabs() << "startElement " << name << llendl;
		for (S32 i = 0; atts[i] && atts[i + 1]; i += 2)
		{
			llinfos << tabs() << "attribute: " << atts[i] << "="
					<< atts[i + 1] << llendl;
		}
	}

	LLXmlTreeNode* child = createXmlTreeNode(name, mCurrent);
	for (S32 i = 0; atts[i] && atts[i + 1]; i += 2)
	{
		child->addAttribute(atts[i], atts[i + 1]);
	}

	if (mCurrent)
	{
		mCurrent->addChild(child);
	}
	else
	{
		llassert(!mRoot);
		mRoot = child;
	}
	mCurrent = child;
}

LLXmlTreeNode* LLXmlTreeParser::createXmlTreeNode(const char* name,
												  LLXmlTreeNode* parent)
{
	return new LLXmlTreeNode(name, parent, mTree);
}

void LLXmlTreeParser::endElement(const char* name)
{
	if (mDump)
	{
		llinfos << tabs() << "endElement " << name << llendl;
	}

	if (mCurrent && !mCurrent->mContents.empty())
	{
		LLStringUtil::trim(mCurrent->mContents);
		LLStringUtil::removeCRLF(mCurrent->mContents);
	}

	mCurrent = mCurrent->getParent();
}

void LLXmlTreeParser::characterData(const char* s, int len)
{
	std::string str;
	if (s)
	{
		str = std::string(s, len);
	}
	if (mDump)
	{
		llinfos << tabs() << "CharacterData " << str << llendl;
	}
	if (mKeepContents)
	{
		mCurrent->appendContents(str);
	}
}

void LLXmlTreeParser::processingInstruction(const char* target,
											const char* data)
{
	if (mDump)
	{
		llinfos << tabs() << "processingInstruction " << data << llendl;
	}
}

void LLXmlTreeParser::comment(const char* data)
{
	if (mDump)
	{
		llinfos << tabs() << "comment " << data << llendl;
	}
}

void LLXmlTreeParser::startCdataSection()
{
	if (mDump)
	{
		llinfos << tabs() << "startCdataSection" << llendl;
	}
}

void LLXmlTreeParser::endCdataSection()
{
	if (mDump)
	{
		llinfos << tabs() << "endCdataSection" << llendl;
	}
}

void LLXmlTreeParser::defaultData(const char* s, int len)
{
	if (mDump)
	{
		std::string str;
		if (s)
		{
			str = std::string(s, len);
		}
		llinfos << tabs() << "defaultData " << str << llendl;
	}
}

void LLXmlTreeParser::unparsedEntityDecl(const char* entity_name,
										 const char* base,
										 const char* system_id,
										 const char* public_id,
										 const char* notation_name)
{
	if (mDump)
	{
		llinfos << tabs() << "Unparsed entity:" << llendl;
		llinfos << tabs() << "    entityName " << entity_name	<< llendl;
		llinfos << tabs() << "    base " << base << llendl;
		llinfos << tabs() << "    systemId " << system_id << llendl;
		llinfos << tabs() << "    publicId " << public_id << llendl;
		llinfos << tabs() << "    notationName " << notation_name << llendl;
	}
}
