/**
 * @file llxmlnode.cpp
 * @author Tom Yedwab
 * @brief LLXMLNode implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llxmlnode.h"

#include "llcolor3.h"
#include "llcolor4.h"
#include "llcolor4u.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llvector4.h"
#include "llquaternion.h"
#include "llstring.h"

// static
bool LLXMLNode::sStripEscapedStrings = true;
bool LLXMLNode::sStripWhitespaceValues = false;

LLXMLNode::LLXMLNode()
:	mParser(NULL),
	mIsAttribute(false),
	mVersionMajor(0),
	mVersionMinor(0),
	mLength(0),
	mPrecision(64),
	mType(TYPE_CONTAINER),
	mEncoding(ENCODING_DEFAULT),
	mLineNumber(-1),
	mParent(NULL),
	mChildren(NULL),
	mAttributes(),
	mPrev(NULL),
	mNext(NULL),
	mName(NULL),
	mDefault(NULL)
{
}

LLXMLNode::LLXMLNode(const char* name, bool is_attribute)
:	mParser(NULL),
	mIsAttribute(is_attribute),
	mVersionMajor(0),
	mVersionMinor(0),
	mLength(0),
	mPrecision(64),
	mType(TYPE_CONTAINER),
	mEncoding(ENCODING_DEFAULT),
	mLineNumber(-1),
	mParent(NULL),
	mChildren(NULL),
	mAttributes(),
	mPrev(NULL),
	mNext(NULL),
	mDefault(NULL)
{
    mName = gStringTable.addStringEntry(name);
}

LLXMLNode::LLXMLNode(LLStringTableEntry* name, bool is_attribute)
:	mParser(NULL),
	mIsAttribute(is_attribute),
	mVersionMajor(0),
	mVersionMinor(0),
	mLength(0),
	mPrecision(64),
	mType(TYPE_CONTAINER),
	mEncoding(ENCODING_DEFAULT),
	mLineNumber(-1),
	mParent(NULL),
	mChildren(NULL),
	mAttributes(),
	mPrev(NULL),
	mNext(NULL),
	mName(name),
	mDefault(NULL)
{
}

// copy constructor (except for the children)
LLXMLNode::LLXMLNode(const LLXMLNode& rhs)
:	mID(rhs.mID),
	mIsAttribute(rhs.mIsAttribute),
	mVersionMajor(rhs.mVersionMajor),
	mVersionMinor(rhs.mVersionMinor),
	mLength(rhs.mLength),
	mPrecision(rhs.mPrecision),
	mType(rhs.mType),
	mEncoding(rhs.mEncoding),
	mLineNumber(0),
	mParser(NULL),
	mParent(NULL),
	mChildren(NULL),
	mAttributes(),
	mPrev(NULL),
	mNext(NULL),
	mName(rhs.mName),
	mValue(rhs.mValue),
	mDefault(rhs.mDefault)
{
}

// returns a new copy of this node and all its children
LLXMLNodePtr LLXMLNode::deepCopy()
{
	LLXMLNodePtr newnode = LLXMLNodePtr(new LLXMLNode(*this));
	if (mChildren.notNull())
	{
		for (LLXMLChildList::iterator it = mChildren->map.begin(),
									  end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr temp_ptr_for_gcc(it->second->deepCopy());
			newnode->addChild(temp_ptr_for_gcc);
		}
	}
	for (LLXMLAttribList::iterator it = mAttributes.begin(),
								   end = mAttributes.end();
		 it != end; ++it)
	{
		LLXMLNodePtr temp_ptr_for_gcc(it->second->deepCopy());
		newnode->addChild(temp_ptr_for_gcc);
	}

	return newnode;
}

// virtual
LLXMLNode::~LLXMLNode()
{
	// Strictly speaking none of this should be required except
	// 'delete mChildren'...
	// Sadly, that is only true if we had not had reference-counted smart
	// pointers linked in three different directions. This entire class is a
	// frightening, hard-to-maintain mess.
	if (mChildren.notNull())
	{
		for (LLXMLChildList::iterator it = mChildren->map.begin(),
									  end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.notNull())
			{
				child->mParent = NULL;
				child->mNext = NULL;
				child->mPrev = NULL;
			}
		}
		mChildren->map.clear();
		mChildren->head = NULL;
		mChildren->tail = NULL;
		mChildren = NULL;
	}
	for (LLXMLAttribList::iterator it = mAttributes.begin(),
								   end = mAttributes.end();
		 it != end; ++it)
	{
		LLXMLNodePtr attr = it->second;
		if (attr.notNull())
		{
			attr->mParent = NULL;
			attr->mNext = NULL;
			attr->mPrev = NULL;
		}
	}
	llassert(mParent == NULL);
	mDefault = NULL;
}

// protected
bool LLXMLNode::removeChild(LLXMLNode* target_child)
{
	if (!target_child)
	{
		return false;
	}
	if (target_child->mIsAttribute)
	{
		LLXMLAttribList::iterator it = mAttributes.find(target_child->mName);
		if (it != mAttributes.end())
		{
			target_child->mParent = NULL;
			mAttributes.erase(it);
			return true;
		}
	}
	else if (mChildren.notNull())
	{
		LLXMLChildList::iterator it = mChildren->map.find(target_child->mName);
		while (it != mChildren->map.end())
		{
			if (target_child == it->second)
			{
				if (target_child == mChildren->head)
				{
					mChildren->head = target_child->mNext;
				}
				if (target_child == mChildren->tail)
				{
					mChildren->tail = target_child->mPrev;
				}

				LLXMLNodePtr prev = target_child->mPrev;
				LLXMLNodePtr next = target_child->mNext;
				if (prev.notNull()) prev->mNext = next;
				if (next.notNull()) next->mPrev = prev;

				target_child->mPrev = NULL;
				target_child->mNext = NULL;
				target_child->mParent = NULL;
				mChildren->map.erase(it);
				if (mChildren->map.empty())
				{
					mChildren = NULL;
				}
				return true;
			}
			else if (it->first != target_child->mName)
			{
				break;
			}
			else
			{
				++it;
			}
		}
	}
	return false;
}

void LLXMLNode::addChild(LLXMLNodePtr new_child, LLXMLNodePtr after_child)
{
	if (new_child.isNull())
	{
		llassert(false);
		return;
	}

	if (new_child->mParent)
	{
		if (new_child->mParent == this)
		{
			return;
		}
		new_child->mParent->removeChild(new_child);
	}

	new_child->mParent = this;
	if (new_child->mIsAttribute)
	{
		mAttributes.emplace(new_child->mName, new_child);
	}
	else
	{
		if (mChildren.isNull())
		{
			mChildren = new LLXMLChildren();
			mChildren->head = new_child;
			mChildren->tail = new_child;
		}
		mChildren->map.emplace(new_child->mName, new_child);

		// if after_child is specified, it damn well better be in the list of
		// children for this node. I am not going to assert that, because it
		// would be expensive, but don't specify that parameter if you did not
		// get the value for it from the list of children of this node!
		if (after_child.isNull())
		{
			if (mChildren->tail != new_child)
			{
				mChildren->tail->mNext = new_child;
				new_child->mPrev = mChildren->tail;
				mChildren->tail = new_child;
			}
		}
		// If after_child == parent, then put new_child at beginning
		else if (after_child == this)
		{
			// Add to front of list
			new_child->mNext = mChildren->head;
			if (mChildren->head)
			{
				mChildren->head->mPrev = new_child;
				mChildren->head = new_child;
			}
			else // no children
			{
				mChildren->head = new_child;
				mChildren->tail = new_child;
			}
		}
		else
		{
			if (after_child->mNext.notNull())
			{
				// If after_child was not the last item, fix up some pointers
				after_child->mNext->mPrev = new_child;
				new_child->mNext = after_child->mNext;
			}
			new_child->mPrev = after_child;
			after_child->mNext = new_child;
			if (mChildren->tail == after_child)
			{
				mChildren->tail = new_child;
			}
		}
	}

	new_child->updateDefault();
}

// virtual
LLXMLNodePtr LLXMLNode::createChild(const char* name, bool is_attribute)
{
	return createChild(gStringTable.addStringEntry(name), is_attribute);
}

// virtual
LLXMLNodePtr LLXMLNode::createChild(LLStringTableEntry* name,
									bool is_attribute)
{
	LLXMLNode* ret = new LLXMLNode(name, is_attribute);
	ret->mID.clear();
	addChild(ret);
	return ret;
}

bool LLXMLNode::deleteChild(LLXMLNode* child)
{
	return removeChild(child);
}

void LLXMLNode::setParent(LLXMLNodePtr new_parent)
{
	if (new_parent.notNull())
	{
		new_parent->addChild(this);
	}
	else if (mParent)
	{
	    LLXMLNodePtr old_parent = mParent;
		mParent = NULL;
		old_parent->removeChild(this);
	}
}

void LLXMLNode::updateDefault()
{
	if (mParent && !mParent->mDefault.isNull())
	{
		mDefault = NULL;

		// Find default value in parent's default tree
		if (!mParent->mDefault.isNull())
		{
			findDefault(mParent->mDefault);
		}
	}

	if (mChildren.notNull())
	{
		for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
											end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.notNull())
			{
				child->updateDefault();
			}
		}
	}
}

void XMLCALL StartXMLNode(void* user_data, const XML_Char* name,
						  const XML_Char** atts)
{
	if (!user_data)
	{
		llwarns << "Parent (user_data) is NULL; aborting." << llendl;
		return;
	}

	// Create a new node
	LLXMLNode* new_node_ptr = new LLXMLNode(name, false);
	LLXMLNodePtr new_node = new_node_ptr;
	new_node->mID.clear();
	LLXMLNodePtr ptr_new_node = new_node;

	// Set the parent-child relationship with the current active node
	LLXMLNode* parent = (LLXMLNode*)user_data;

	new_node_ptr->mParser = parent->mParser;
	new_node_ptr->setLineNumber(XML_GetCurrentLineNumber(*new_node_ptr->mParser));

	// Set the current active node to the new node
	XML_Parser* parser = parent->mParser;
	XML_SetUserData(*parser, (void*)new_node_ptr);

	// Parse attributes
	U32 pos = 0;
	std::string attr_name, attr_value;
	while (atts[pos])
	{
		attr_name = atts[pos];
		attr_value = atts[pos + 1];

		// Special cases
		if ('i' == attr_name[0] && "id" == attr_name)
		{
			new_node->mID = attr_value;
		}
		else if ('v' == attr_name[0] && "version" == attr_name)
		{
			U32 version_major = 0;
			U32 version_minor = 0;
			if (sscanf(attr_value.c_str(), "%d.%d", &version_major,
					   &version_minor) > 0)
			{
				new_node->mVersionMajor = version_major;
				new_node->mVersionMinor = version_minor;
			}
		}
		else if (('s' == attr_name[0] && "size" == attr_name) ||
				 ('l' == attr_name[0] && "length" == attr_name))
		{
			U32 length;
			if (sscanf(attr_value.c_str(), "%d", &length) > 0)
			{
				new_node->mLength = length;
			}
		}
		else if ('p' == attr_name[0] && "precision" == attr_name)
		{
			U32 precision;
			if (sscanf(attr_value.c_str(), "%d", &precision) > 0)
			{
				new_node->mPrecision = precision;
			}
		}
		else if ('t' == attr_name[0] && "type" == attr_name)
		{
			if ("boolean" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_BOOLEAN;
			}
			else if ("integer" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_INTEGER;
			}
			else if ("float" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_FLOAT;
			}
			else if ("string" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_STRING;
			}
			else if ("uuid" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_UUID;
			}
			else if ("noderef" == attr_value)
			{
				new_node->mType = LLXMLNode::TYPE_NODEREF;
			}
		}
		else if ('e' == attr_name[0] && "encoding" == attr_name)
		{
			if ("decimal" == attr_value)
			{
				new_node->mEncoding = LLXMLNode::ENCODING_DECIMAL;
			}
			else if ("hex" == attr_value)
			{
				new_node->mEncoding = LLXMLNode::ENCODING_HEX;
			}
			/*else if (attr_value == "base32")
			{
				new_node->mEncoding = LLXMLNode::ENCODING_BASE32;
			}*/
		}

		// only one attribute child per description
		LLXMLNodePtr attr_node;
		if (!new_node->getAttribute(attr_name.c_str(), attr_node, false))
		{
			attr_node = new LLXMLNode(attr_name.c_str(), true);
			attr_node->setLineNumber(XML_GetCurrentLineNumber(*new_node_ptr->mParser));
		}
		attr_node->setValue(attr_value);
		new_node->addChild(attr_node);

		pos += 2;
	}

	if (parent)
	{
		parent->addChild(new_node);
	}
}

void XMLCALL EndXMLNode(void* user_data, const XML_Char* name)
{
	if (!user_data)
	{
		llwarns << "Node (user_data) is NULL; aborting." << llendl;
		return;
	}

	// [FUGLY] Set the current active node to the current node's parent
	LLXMLNode* node = (LLXMLNode*)user_data;
	XML_Parser* parser = node->mParser;
	XML_SetUserData(*parser, (void*)node->mParent);
	// SJB: total hack:
	if (LLXMLNode::sStripWhitespaceValues)
	{
		std::string value = node->getValue();
		bool is_empty = true;
		for (size_t s = 0; s < value.length(); ++s)
		{
			char c = value[s];
			if (c != ' ' && c != '\t' && c != '\n')
			{
				is_empty = false;
				break;
			}
		}
		if (is_empty)
		{
			value.clear();
			node->setValue(value);
		}
	}
}

void XMLCALL XMLData(void* user_data, const XML_Char* s, int len)
{
	if (!user_data)
	{
		llwarns << "Node (user_data) is NULL; aborting." << llendl;
		return;
	}

	LLXMLNode* current_node = (LLXMLNode*)user_data;
	std::string value = current_node->getValue();
	if (LLXMLNode::sStripEscapedStrings)
	{
		if (s[0] == '\"' && s[len - 1] == '\"')
		{
			// Special-case: Escaped string.
			std::string unescaped_string;
			for (S32 pos = 1; pos < len - 1; ++pos)
			{
				if (s[pos] == '\\' && s[pos + 1] == '\\')
				{
					unescaped_string.append("\\");
					++pos;
				}
				else if (s[pos] == '\\' && s[pos + 1] == '\"')
				{
					unescaped_string.append("\"");
					++pos;
				}
				else
				{
					unescaped_string.append(&s[pos], 1);
				}
			}
			value.append(unescaped_string);
			current_node->setValue(value);
			return;
		}
	}
	value.append(std::string(s, len));
	current_node->setValue(value);
}

// static
bool LLXMLNode::updateNode(LLXMLNodePtr& node, LLXMLNodePtr& update_node)
{

	if (node.isNull() || update_node.isNull())
	{
		llwarns << "Invalid node. Skipping." << llendl;
		return false;
	}

	// update the node value
	node->mValue = update_node->mValue;

	// update all attribute values
	for (LLXMLAttribList::const_iterator it = update_node->mAttributes.begin(),
										 end = update_node->mAttributes.end();
		 it != end; ++it)
	{
		const LLStringTableEntry* attrib_name_entry = it->first;
		LLXMLNodePtr update_attrib_node = it->second;

		LLXMLNodePtr attrib_node;
		node->getAttribute(attrib_name_entry, attrib_node, 0);
		if (attrib_node)
		{
			attrib_node->mValue = update_attrib_node->mValue;
		}
	}

	// update all of node's children with updateNodes children that match name
	LLXMLNodePtr child = node->getFirstChild();
	LLXMLNodePtr last_child = child;
	std::string node_name, update_name;
	for (LLXMLNodePtr update_child = update_node->getFirstChild();
		 update_child.notNull(); update_child = update_child->getNextSibling())
	{
		while (child.notNull())
		{
			update_child->getAttributeString("name", update_name);
			child->getAttributeString("name", node_name);

			// if it is a combobox there's no name, but there is a value
			if (update_name.empty())
			{
				update_child->getAttributeString("value", update_name);
				child->getAttributeString("value", node_name);
			}

			if (!node_name.empty() && update_name == node_name)
			{
				updateNode(child, update_child);
				last_child = child;
				child = child->getNextSibling();
				if (child.isNull())
				{
					child = node->getFirstChild();
				}
				break;
			}

			child = child->getNextSibling();
			if (child.isNull())
			{
				child = node->getFirstChild();
			}
			if (child == last_child)
			{
				 break;
			}
		}
	}

	return true;
}

// static
LLXMLNodePtr LLXMLNode::replaceNode(LLXMLNodePtr node,
									LLXMLNodePtr update_node)
{
	if (node.isNull() || update_node.isNull())
	{
		llwarns << "Node invalid" << llendl;
		return node;
	}

	LLXMLNodePtr cloned_node = update_node->deepCopy();
	node->mParent->addChild(cloned_node, node);	// add after node
	LLXMLNodePtr parent = node->mParent;
	parent->removeChild(node);
	parent->updateDefault();

	return cloned_node;
}

// static
bool LLXMLNode::parseFile(const std::string& filename, LLXMLNodePtr& node,
						  LLXMLNode* defaults_tree)
{
	// Read file
	LL_DEBUGS("XMLNode") << "parsing XML file: " << filename << LL_ENDL;
	LLFILE* fp = LLFile::open(filename, "rb");
	if (!fp)
	{
		node = NULL;
		return false;
	}
	fseek(fp, 0, SEEK_END);
	U32 length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	U8* buffer = new U8[length + 1];
	size_t nread = fread(buffer, 1, length, fp);
	buffer[nread] = 0;
	LLFile::close(fp);

	bool rv = parseBuffer(buffer, nread, node, defaults_tree);
	delete[] buffer;
	return rv;
}

// static
bool LLXMLNode::parseBuffer(U8* buffer, U32 length, LLXMLNodePtr& node,
							LLXMLNode* defaults)
{
	// Init
	XML_Parser my_parser = XML_ParserCreate(NULL);
	XML_SetElementHandler(my_parser, StartXMLNode, EndXMLNode);
	XML_SetCharacterDataHandler(my_parser, XMLData);

	// Create a root node
	LLXMLNode* file_node_ptr = new LLXMLNode("XML", false);
	LLXMLNodePtr file_node = file_node_ptr;
	file_node->mParser = &my_parser;
	XML_SetUserData(my_parser, (void*)file_node_ptr);

	// Do the parsing
	if (XML_Parse(my_parser, (const char*)buffer, length,
				  true) != XML_STATUS_OK)
	{
		std::string tmp((const char*)buffer, length);
		llwarns << "Error parsing XML. Error code: "
				<< XML_ErrorString(XML_GetErrorCode(my_parser))
				<< " at line " << XML_GetCurrentLineNumber(my_parser)
				<< " - Parsed buffer:\n" << tmp << llendl;
	}

	// Deinit
	XML_ParserFree(my_parser);

	if (!file_node->mChildren || file_node->mChildren->map.size() != 1)
	{
		llwarns << "Parse failure - wrong number of top-level nodes."
				<< llendl;
		node = NULL;
		return false;
	}

	LLXMLNode* return_node = file_node->mChildren->map.begin()->second;
	if (!return_node)
	{
		llwarns << "Parse failure - Could not allocate a new node !" << llendl;
		return false;
	}

	return_node->setDefault(defaults);
	return_node->updateDefault();

	node = return_node;
	return true;
}

// static
bool LLXMLNode::parseStream(std::istream& str, LLXMLNodePtr& node,
							LLXMLNode* defaults)
{
	// Init
	XML_Parser my_parser = XML_ParserCreate(NULL);
	XML_SetElementHandler(my_parser, StartXMLNode, EndXMLNode);
	XML_SetCharacterDataHandler(my_parser, XMLData);

	// Create a root node
	LLXMLNode* file_node_ptr = new LLXMLNode("XML", false);
	LLXMLNodePtr file_node = file_node_ptr;
	file_node->mParser = &my_parser;
	XML_SetUserData(my_parser, (void*)file_node_ptr);

	constexpr int BUFSIZE = 1024;
	U8* buffer = new U8[BUFSIZE];
	while (str.good())
	{
		str.read((char*)buffer, BUFSIZE);
		int count = (int)str.gcount();

		if (XML_Parse(my_parser, (const char*)buffer, count,
					  !str.good()) != XML_STATUS_OK)
		{
			llwarns << "Error parsing XML. Error code: "
					<< XML_ErrorString(XML_GetErrorCode(my_parser))
					<< " at line " << XML_GetCurrentLineNumber(my_parser)
					<< llendl;
			break;
		}
	}
	delete[] buffer;

	// Deinit
	XML_ParserFree(my_parser);

	if (!file_node->mChildren || file_node->mChildren->map.size() != 1)
	{
		llwarns << "Parse failure - wrong number of top-level nodes."
				<< llendl;
		node = NULL;
		return false;
	}

	LLXMLNode* return_node = file_node->mChildren->map.begin()->second;
	if (!return_node)
	{
		llwarns << "Parse failure - Could not allocate a new node !" << llendl;
		node = NULL;
		return false;
	}

	return_node->setDefault(defaults);
	return_node->updateDefault();

	node = return_node;
	return true;
}

bool LLXMLNode::isFullyDefault()
{
	if (mDefault.isNull())
	{
		return false;
	}
	bool has_default_value = mValue == mDefault->mValue;
	bool has_default_attribute = mIsAttribute == mDefault->mIsAttribute;
	bool has_default_type = mIsAttribute || mType == mDefault->mType;
	bool has_default_encoding = mIsAttribute || mEncoding == mDefault->mEncoding;
	bool has_default_precision = mIsAttribute || mPrecision == mDefault->mPrecision;
	bool has_default_length = mIsAttribute || mLength == mDefault->mLength;

	if (has_default_value && has_default_type && has_default_encoding &&
		has_default_precision && has_default_length && has_default_attribute)
	{
		if (mChildren.notNull())
		{
			for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
												end = mChildren->map.end();
				 it != end; ++it)
			{
				LLXMLNodePtr child = it->second;
				if (child.notNull() && !child->isFullyDefault())
				{
					return false;
				}
			}
		}
		return true;
	}

	return false;
}

// static
bool LLXMLNode::getLayeredXMLNode(LLXMLNodePtr& root,
								  const std::vector<std::string>& paths)
{
	if (paths.empty()) return false;

	std::vector<std::string>::const_iterator it = paths.begin();
	const std::string& filename = *it;
	if (filename.empty())
	{
		return false;
	}

	if (!LLXMLNode::parseFile(filename, root, NULL))
	{
		llwarns << "Problem reading UI description file: " << filename
				<< llendl;
		return false;
	}

	LLXMLNodePtr update_root;
	std::string node_name, update_name;
	std::vector<std::string>::const_iterator end = paths.end();
	while (++it != end)
	{
		const std::string& layer_filename = *it;
		if (layer_filename.empty() || layer_filename == filename)
		{
			// no localized version of this file, that's ok, keep looking
			continue;
		}

		if (!LLXMLNode::parseFile(layer_filename, update_root, NULL))
		{
			llwarns << "Problem reading localized UI description file: "
					<< layer_filename << llendl;
			return false;
		}

		update_root->getAttributeString("name", update_name);
		root->getAttributeString("name", node_name);
		if (update_name == node_name)
		{
			LLXMLNode::updateNode(root, update_root);
		}
	}

	return true;
}

// static
void LLXMLNode::writeHeaderToFile(LLFILE* out_file)
{
	fprintf(out_file,
			"<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?>\n");
}

void LLXMLNode::writeToFile(LLFILE* out_file,
							const std::string& indent,
							bool use_type_decorations)
{
	if (isFullyDefault())
	{
		// Don't write out nodes that are an exact match to defaults
		return;
	}

	std::ostringstream ostream;
	writeToOstream(ostream, indent, use_type_decorations);
	std::string outstring = ostream.str();
	size_t written = fwrite(outstring.c_str(), 1, outstring.length(),
							out_file);
	if (written != outstring.length())
	{
		llwarns << "Short write" << llendl;
	}
}

void LLXMLNode::writeToOstream(std::ostream& output_stream,
							   const std::string& indent,
							   bool use_type_decorations)
{
	if (isFullyDefault())
	{
		// Don't write out nodes that are an exact match to defaults
		LL_DEBUGS("XMLNode") << "Node "
							 << std::string(mName ? mName->mString : "no name")
							 << " is full default, not writing." << LL_ENDL;
		return;
	}

	if (!mName)
	{
		llwarns << "No name for node. Skipping." << llendl;
		return;
	}

	bool has_default = mDefault.notNull();
	bool has_default_type = has_default && mType == mDefault->mType;
	bool has_default_encoding = has_default && mEncoding == mDefault->mEncoding;
	bool has_default_precision = has_default && mPrecision == mDefault->mPrecision;
	bool has_default_length = has_default && mLength == mDefault->mLength;

	// stream the name
	output_stream << indent << "<" << mName->mString << "\n";

	if (use_type_decorations)
	{
		LL_DEBUGS("XMLNode") << "Writing decorations for node: "
							 << mName->mString << LL_ENDL;
		// ID
		if (!mID.empty())
		{
			LL_DEBUGS("XMLNode") << " - Id: " << mID << LL_ENDL;
			output_stream << indent << " id=\"" << mID << "\"\n";
		}

		// Type
		if (!has_default_type)
		{
			switch (mType)
			{
				case TYPE_BOOLEAN:
					LL_DEBUGS("XMLNode") << " - Type: boolean" << LL_ENDL;
					output_stream << indent << " type=\"boolean\"\n";
					break;
				case TYPE_INTEGER:
					LL_DEBUGS("XMLNode") << " - Type: integer" << LL_ENDL;
					output_stream << indent << " type=\"integer\"\n";
					break;
				case TYPE_FLOAT:
					LL_DEBUGS("XMLNode") << " - Type: float" << LL_ENDL;
					output_stream << indent << " type=\"float\"\n";
					break;
				case TYPE_STRING:
					LL_DEBUGS("XMLNode") << " - Type: string" << LL_ENDL;
					output_stream << indent << " type=\"string\"\n";
					break;
				case TYPE_UUID:
					LL_DEBUGS("XMLNode") << " - Type: UUID" << LL_ENDL;
					output_stream << indent << " type=\"uuid\"\n";
					break;
				case TYPE_NODEREF:
					LL_DEBUGS("XMLNode") << " - Type: node ref" << LL_ENDL;
					output_stream << indent << " type=\"noderef\"\n";
					break;
				default:
					break;
			}
		}

		// Encoding
		if (!has_default_encoding)
		{
			switch (mEncoding)
			{
				case ENCODING_DECIMAL:
					LL_DEBUGS("XMLNode") << " - Encoding: decimal" << LL_ENDL;
					output_stream << indent << " encoding=\"decimal\"\n";
					break;
				case ENCODING_HEX:
					LL_DEBUGS("XMLNode") << " - Encoding: hexadecimal"
										 << LL_ENDL;
					output_stream << indent << " encoding=\"hex\"\n";
					break;
#if 0
				case ENCODING_BASE32:
					output_stream << indent << " encoding=\"base32\"\n";
					break;
#endif
				default:
					break;
			}
		}

		// Precision
		if (!has_default_precision &&
			(mType == TYPE_INTEGER || mType == TYPE_FLOAT))
		{
			LL_DEBUGS("XMLNode") << " - Precision: " << mPrecision << LL_ENDL;
			output_stream << indent << " precision=\"" << mPrecision << "\"\n";
		}

		// Version
		if (mVersionMajor > 0 || mVersionMinor > 0)
		{
			LL_DEBUGS("XMLNode") << " - Version: " << mVersionMajor << "."
								 << mVersionMinor << LL_ENDL;
			output_stream << indent << " version=\"" << mVersionMajor << "."
						  << mVersionMinor << "\"\n";
		}

		// Array length
		if (!has_default_length && mLength > 0)
		{
			LL_DEBUGS("XMLNode") << " - Length: " << mLength << LL_ENDL;
			output_stream << indent << " length=\"" << mLength << "\"\n";
		}
	}

	{
		// Write out attributes
		LL_DEBUGS("XMLNode") << "Writing attributes for node: "
							 << mName->mString << LL_ENDL;
		std::string attr_str;
		for (LLXMLAttribList::const_iterator it = mAttributes.begin(),
											 end = mAttributes.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.isNull() || !child->mName) continue;

			LL_DEBUGS("XMLNode") << "Child: " << child->mName->mString
								 << LL_ENDL;

			if (child->mDefault.isNull() ||
				child->mDefault->mValue != child->mValue)
			{
				std::string attr = child->mName->mString;
				if (use_type_decorations &&
					(attr == "id" || attr == "type" || attr == "encoding" ||
					 attr == "precision" || attr == "version" ||
					 attr == "length"))
				{
					continue; // skip built-in attributes
				}

				attr_str = llformat(" %s=\"%s\"", attr.c_str(),
									escapeXML(child->mValue).c_str());
				LL_DEBUGS("XMLNode") << " - attribute: " << attr_str
									 << LL_ENDL;
				output_stream << indent << attr_str << "\n";
			}
		}
	}

	// erase last \n before attaching final > or />
	output_stream.seekp(-1, std::ios::cur);

	if (mChildren.isNull() && mValue.empty())
	{
		output_stream << " />\n";
		return;
	}
	else
	{
		output_stream << ">\n";
		if (mChildren.notNull())
		{
			// stream non-attributes
			std::string next_indent = indent + "    ";
			for (LLXMLNode* child = getFirstChild(); child;
				 child = child->getNextSibling())
			{
				child->writeToOstream(output_stream, next_indent,
									  use_type_decorations);
			}
		}
		if (!mValue.empty())
		{
			std::string contents = getTextContents();
			output_stream << indent << "    " << escapeXML(contents) << "\n";
			LL_DEBUGS("XMLNode") << "Value: " << escapeXML(contents)
								 << LL_ENDL;
		}
		output_stream << indent << "</" << mName->mString << ">\n";
	}
	LL_DEBUGS("XMLNode") << "Finished writing data for node: "
						 << mName->mString << LL_ENDL;
}

void LLXMLNode::findName(const std::string& name, LLXMLNodeList& results)
{
    LLStringTableEntry* name_entry = gStringTable.checkStringEntry(name);
	if (name_entry == mName)
	{
		results.emplace(mName->mString, this);
	}
	else if (mChildren.notNull())
	{
		for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
											end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.notNull())
			{
				child->findName(name_entry, results);
			}
		}
	}
}

void LLXMLNode::findName(LLStringTableEntry* name, LLXMLNodeList& results)
{
	if (name == mName)
	{
		results.emplace(mName->mString, this);
	}
	else if (mChildren.notNull())
	{
		for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
											end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.notNull())
			{
				child->findName(name, results);
			}
		}
	}
}

void LLXMLNode::findID(const std::string& id, LLXMLNodeList& results)
{
	if (id == mID)
	{
		results.emplace(mName->mString, this);
	}
	else if (mChildren.notNull())
	{
		for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
											end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.notNull())
			{
				child->findID(id, results);
			}
		}
	}
}

void LLXMLNode::scrubToTree(LLXMLNode* tree)
{
	if (tree && tree->mChildren.notNull() && mChildren.notNull())
	{
		std::vector<LLXMLNodePtr> to_delete_list;
		LLXMLChildList::iterator it = mChildren->map.begin();
		while (it != mChildren->map.end())
		{
			LLXMLNodePtr child = it->second;
			if (child.isNull()) continue;	// Paranoia

			LLXMLNodePtr child_tree = NULL;
			// Look for this child in the default's children
			bool found = false;
			LLXMLChildList::iterator it2 = tree->mChildren->map.begin();
			while (it2 != tree->mChildren->map.end())
			{
				if (child->mName == it2->second->mName)
				{
					child_tree = it2->second;
					found = true;
				}
				++it2;
			}
			if (!found)
			{
				to_delete_list.push_back(child);
			}
			else
			{
				child->scrubToTree(child_tree);
			}
			++it;
		}
		for (std::vector<LLXMLNodePtr>::iterator it3 = to_delete_list.begin(),
												 end3 = to_delete_list.end();
			 it3 != end3; ++it3)
		{
			(*it3)->setParent(NULL);
		}
	}
}

bool LLXMLNode::getChild(const char* name,
						 LLXMLNodePtr& node,
						 bool use_default_if_missing)
{
    return getChild(gStringTable.checkStringEntry(name), node,
					use_default_if_missing);
}

bool LLXMLNode::getChild(const LLStringTableEntry* name,
						 LLXMLNodePtr& node,
						 bool use_default_if_missing)
{
	if (mChildren.notNull())
	{
		LLXMLChildList::const_iterator it = mChildren->map.find(name);
		if (it != mChildren->map.end())
		{
			node = it->second;
			return true;
		}
	}
	if (use_default_if_missing && !mDefault.isNull())
	{
		return mDefault->getChild(name, node, false);
	}
	node = NULL;
	return false;
}

void LLXMLNode::getChildren(const char* name,
							LLXMLNodeList& children,
							bool use_default_if_missing) const
{
    getChildren(gStringTable.checkStringEntry(name), children,
				use_default_if_missing);
}

void LLXMLNode::getChildren(const LLStringTableEntry* name,
							LLXMLNodeList& children,
							bool use_default_if_missing) const
{
	if (mChildren.notNull())
	{
		LLXMLChildList::const_iterator end = mChildren->map.end();
		LLXMLChildList::const_iterator it = mChildren->map.find(name);
		if (it != end)
		{
			while (it != end)
			{
				LLXMLNodePtr child = (it++)->second;
				if (child.isNull() || !child->mName) continue;

				if (name != child->mName)
				{
					break;
				}
				children.emplace(child->mName->mString, child);
			}
		}
	}
	if (children.empty() && use_default_if_missing && !mDefault.isNull())
	{
		mDefault->getChildren(name, children, false);
	}
}

// recursively walks the tree and returns all children at all nesting levels
// matching the name
void LLXMLNode::getDescendants(const LLStringTableEntry* name,
							   LLXMLNodeList& children) const
{
	if (mChildren.notNull())
	{
		for (LLXMLChildList::const_iterator it = mChildren->map.begin(),
											end = mChildren->map.end();
			 it != end; ++it)
		{
			LLXMLNodePtr child = it->second;
			if (child.isNull() || !child->mName) continue;

			if (name == child->mName)
			{
				children.emplace(child->mName->mString, child);
			}
			// and check each child as well
			child->getDescendants(name, children);
		}
	}
}

bool LLXMLNode::getAttribute(const char* name,
							 LLXMLNodePtr& node,
							 bool use_default_if_missing)
{
    return getAttribute(gStringTable.checkStringEntry(name), node,
						use_default_if_missing);
}

bool LLXMLNode::getAttribute(const LLStringTableEntry* name,
							 LLXMLNodePtr& node,
							 bool use_default_if_missing)
{
	LLXMLAttribList::const_iterator it = mAttributes.find(name);
	if (it != mAttributes.end())
	{
		node = it->second;
		return true;
	}
	if (use_default_if_missing && !mDefault.isNull())
	{
		return mDefault->getAttribute(name, node, false);
	}

	return false;
}

bool LLXMLNode::setAttributeString(const char* attr, const std::string& value)
{
	LLStringTableEntry* name = gStringTable.checkStringEntry(attr);
	LLXMLAttribList::const_iterator it = mAttributes.find(name);
	if (it != mAttributes.end())
	{
		LLXMLNodePtr node = it->second;
		if (node.notNull())
		{
			node->setValue(value);
			return true;
		}
	}
	return false;
}

bool LLXMLNode::hasAttribute(const char* name)
{
	LLXMLNodePtr node;
	return getAttribute(name, node);
}

bool LLXMLNode::getAttributeBool(const char* name, bool& value)
{
	LLXMLNodePtr node;
	return getAttribute(name, node) && node->getBoolValue(1, &value);
}

bool LLXMLNode::getAttributeU8(const char* name, U8& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getByteValue(1, &value));
}

bool LLXMLNode::getAttributeS8(const char* name, S8& value)
{
	LLXMLNodePtr node;
	S32 val;
	if (!(getAttribute(name, node) && node->getIntValue(1, &val)))
	{
		return false;
	}
	value = val;
	return true;
}

bool LLXMLNode::getAttributeU16(const char* name, U16& value)
{
	LLXMLNodePtr node;
	U32 val;
	if (!(getAttribute(name, node) && node->getUnsignedValue(1, &val)))
	{
		return false;
	}
	value = val;
	return true;
}

bool LLXMLNode::getAttributeS16(const char* name, S16& value)
{
	LLXMLNodePtr node;
	S32 val;
	if (!(getAttribute(name, node) && node->getIntValue(1, &val)))
	{
		return false;
	}
	value = val;
	return true;
}

bool LLXMLNode::getAttributeU32(const char* name, U32& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getUnsignedValue(1, &value));
}

bool LLXMLNode::getAttributeS32(const char* name, S32& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getIntValue(1, &value));
}

bool LLXMLNode::getAttributeF32(const char* name, F32& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getFloatValue(1, &value));
}

bool LLXMLNode::getAttributeF64(const char* name, F64& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getDoubleValue(1, &value));
}

bool LLXMLNode::getAttributeColor(const char* name, LLColor4& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getFloatValue(4, value.mV));
}

bool LLXMLNode::getAttributeColor4(const char* name, LLColor4& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getFloatValue(4, value.mV));
}

bool LLXMLNode::getAttributeColor4U(const char* name, LLColor4U& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getByteValue(4, value.mV));
}

bool LLXMLNode::getAttributeVector3(const char* name, LLVector3& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getFloatValue(3, value.mV));
}

bool LLXMLNode::getAttributeVector3d(const char* name, LLVector3d& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getDoubleValue(3, value.mdV));
}

bool LLXMLNode::getAttributeQuat(const char* name, LLQuaternion& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getFloatValue(4, value.mQ));
}

bool LLXMLNode::getAttributeUUID(const char* name, LLUUID& value)
{
	LLXMLNodePtr node;
	return (getAttribute(name, node) && node->getUUIDValue(1, &value));
}

bool LLXMLNode::getAttributeString(const char* name, std::string& value)
{
	LLXMLNodePtr node;
	if (!getAttribute(name, node))
	{
		return false;
	}
	value = node->getValue();
	return true;
}

LLXMLNodePtr LLXMLNode::getRoot()
{
	if (mParent)
	{
		return mParent->getRoot();
	}
	return this;
}

//static
const char* LLXMLNode::skipWhitespace(const char* str)
{
	if (!str) return NULL;

	// skip whitespace characters
	while (str[0] == ' ' || str[0] == '\t' || str[0] == '\n')
	{
		++str;
	}
	return str;
}

//static
const char* LLXMLNode::skipNonWhitespace(const char* str)
{
	if (!str) return NULL;

	// skip non-whitespace characters
	while (str[0] != ' ' && str[0] != '\t' && str[0] != '\n' && str[0] != 0)
	{
		++str;
	}
	return str;
}

//static
const char* LLXMLNode::parseInteger(const char* str, U64* dest,
									bool* is_negative, U32 precision,
									Encoding encoding)
{
	*dest = 0;
	*is_negative = false;

	str = skipWhitespace(str);

	if (str[0] == 0) return NULL;

	if (encoding == ENCODING_DECIMAL || encoding == ENCODING_DEFAULT)
	{
		if (str[0] == '+')
		{
			++str;
		}
		if (str[0] == '-')
		{
			*is_negative = true;
			++str;
		}

		str = skipWhitespace(str);

		U64 ret = 0;
		while (str[0] >= '0' && str[0] <= '9')
		{
			ret *= 10;
			ret += str[0] - '0';
			++str;
		}

		if (str[0] == '.')
		{
			// If there is a fractional part, skip it
			str = skipNonWhitespace(str);
		}

		*dest = ret;
		return str;
	}
	if (encoding == ENCODING_HEX)
	{
		U64 ret = 0;
		str = skipWhitespace(str);
		for (U32 pos = 0; pos < precision / 4; ++pos)
		{
			ret <<= 4;
			str = skipWhitespace(str);
			if (str[0] >= '0' && str[0] <= '9')
			{
				ret += str[0] - '0';
			}
			else if (str[0] >= 'a' && str[0] <= 'f')
			{
				ret += str[0] - 'a' + 10;
			}
			else if (str[0] >= 'A' && str[0] <= 'F')
			{
				ret += str[0] - 'A' + 10;
			}
			else
			{
				return NULL;
			}
			++str;
		}

		*dest = ret;
		return str;
	}
	return NULL;
}

// 25 elements - decimal expansions of 1/(2^n), multiplied by 10 each iteration
static const U64 float_coeff_table[] =
{	5, 25, 125, 625, 3125,
	15625, 78125, 390625, 1953125, 9765625,
	48828125, 244140625, 1220703125, 6103515625LL, 30517578125LL,
	152587890625LL, 762939453125LL, 3814697265625LL, 19073486328125LL, 95367431640625LL,
	476837158203125LL, 2384185791015625LL, 11920928955078125LL, 59604644775390625LL, 298023223876953125LL
};

// 36 elements - decimal expansions of 1/(2^n) after the last 28, truncated, no
// multiply each iteration
static const U64 float_coeff_table_2[] =
{	149011611938476562LL,	74505805969238281LL,
	37252902984619140LL,	18626451492309570LL,	9313225746154785LL,	4656612873077392LL,
	2328306436538696LL,		1164153218269348LL,		582076609134674LL,	291038304567337LL,
	145519152283668LL,		72759576141834LL,		36379788070917LL,	18189894035458LL,
	9094947017729LL,		4547473508864LL,		2273736754432LL,	1136868377216LL,
	568434188608LL,			284217094304LL,			142108547152LL,		71054273576LL,
	35527136788LL,			17763568394LL,			8881784197LL,		4440892098LL,
	2220446049LL,			1110223024LL,			555111512LL,		277555756LL,
	138777878,				69388939,				34694469,			17347234,
	8673617,				4336808,				2168404,			1084202,
	542101,					271050,					135525,				67762,
};

//static
const char* LLXMLNode::parseFloat(const char* str, F64* dest, U32 precision,
								  Encoding encoding)
{
	str = skipWhitespace(str);
	if (!str[0]) return NULL;

	if (encoding == ENCODING_DECIMAL || encoding == ENCODING_DEFAULT)
	{
		str = skipWhitespace(str);

		if (strncmp(str, "inf", 3) == 0)
		{
			*(U64*)dest = 0x7FF0000000000000ll;
			return str + 3;
		}
		if (strncmp(str, "-inf", 4) == 0)
		{
			*(U64*)dest = 0xFFF0000000000000ll;
			return str + 4;
		}
		if (strncmp(str, "1.#INF", 6) == 0)
		{
			*(U64*)dest = 0x7FF0000000000000ll;
			return str + 6;
		}
		if (strncmp(str, "-1.#INF", 7) == 0)
		{
			*(U64*)dest = 0xFFF0000000000000ll;
			return str + 7;
		}

		F64 negative = 1.0;
		if (str[0] == '+')
		{
			++str;
		}
		if (str[0] == '-')
		{
			negative = -1.0;
			++str;
		}

		const char* base_str = str;
		str = skipWhitespace(str);

		// Parse the integer part of the expression
		U64 int_part = 0;
		while (str[0] >= '0' && str[0] <= '9')
		{
			int_part *= 10;
			int_part += U64(str[0] - '0');
			++str;
		}

		U64 f_part = 0;
		if (str[0] == '.')
		{
			++str;
			U64 remainder = 0;
			U32 pos = 0;
			// Parse the decimal part of the expression
			while (str[0] >= '0' && str[0] <= '9' && pos < 25)
			{
				remainder = remainder * 10 + U64(str[0] - '0');
				f_part <<= 1;
				// Check the n'th bit
				if (remainder >= float_coeff_table[pos])
				{
					remainder -= float_coeff_table[pos];
					f_part |= 1;
				}
				++pos;
				++str;
			}
			if (pos == 25)
			{
				// Drop any excessive digits
				while (str[0] >= '0' && str[0] <= '9')
				{
					++str;
				}
			}
			else
			{
				while (pos < 25)
				{
					remainder *= 10;
					f_part <<= 1;
					// Check the n'th bit
					if (remainder >= float_coeff_table[pos])
					{
						remainder -= float_coeff_table[pos];
						f_part |= 1;
					}
					++pos;
				}
			}
			pos = 0;
			while (pos < 36)
			{
				f_part <<= 1;
				if (remainder >= float_coeff_table_2[pos])
				{
					remainder -= float_coeff_table_2[pos];
					f_part |= 1;
				}
				++pos;
			}
		}

		F64 ret = F64(int_part) + (F64(f_part) / F64(1LL << 61));

		F64 exponent = 1.0;
		if (str[0] == 'e')
		{
			// Scientific notation
			U64 exp;
			bool is_negative;
			str = parseInteger(++str, &exp, &is_negative, 64,
							   ENCODING_DECIMAL);
			if (!str)
			{
				exp = 1;
			}
			F64 exp_d = is_negative ? -F64(exp) : F64(exp);
			exponent = pow(10.0, exp_d);
		}

		if (str == base_str)
		{
			// no digits parsed
			return NULL;
		}
		else
		{
			*dest = ret * negative * exponent;
			return str;
		}
	}
	if (encoding == ENCODING_HEX)
	{
		U64 bytes_dest;
		bool is_negative;
		str = parseInteger(str, (U64*)&bytes_dest, &is_negative, precision,
						   ENCODING_HEX);
		// Upcast to F64
		switch (precision)
		{
			case 32:
			{
				U32 short_dest = (U32)bytes_dest;
				F32 ret_val = *(F32*)&short_dest;
				*dest = ret_val;
				break;
			}

			case 64:
				*dest = *(F64*)&bytes_dest;
				break;

			default:
				return NULL;
		}

		return str;
	}
	return NULL;
}

U32 LLXMLNode::getBoolValue(U32 expected_length, bool* array)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept booleans or strings
	if (mType != TYPE_BOOLEAN && mType != TYPE_STRING && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	std::string* str_array = new std::string[expected_length];
	U32 length = getStringValue(expected_length, str_array);

	U32 ret_length = 0;
	for (U32 i = 0; i < length; ++i)
	{
		LLStringUtil::toLower(str_array[i]);
		if (str_array[i] == "false")
		{
			array[ret_length++] = false;
		}
		else if (str_array[i] == "true")
		{
			array[ret_length++] = true;
		}
	}

	delete[] str_array;

	if (ret_length != expected_length)
	{
		LL_DEBUGS("XMLNode") << "Failure to get bool for node named '"
							 << mName->mString << "'. Expected "
							 << expected_length << " but only found "
							 << ret_length << LL_ENDL;
	}

	return ret_length;
}

U32 LLXMLNode::getByteValue(U32 expected_length, U8* array, Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept bytes or integers (below 256 only)
	if (mType != TYPE_INTEGER
		&& mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i;
	for (i = 0; i < expected_length; ++i)
	{
		U64 value;
		bool is_negative;
		value_string = parseInteger(value_string, &value, &is_negative, 8,
									encoding);
		if (value_string == NULL)
		{
			break;
		}
		if (value > 255 || is_negative)
		{
			llwarns << "value outside of valid range." << llendl;
			break;
		}
		array[i] = U8(value);
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getIntValue(U32 expected_length, S32* array, Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept bytes or integers
	if (mType != TYPE_INTEGER && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i = 0;
	for (i = 0; i < expected_length; ++i)
	{
		U64 value;
		bool is_negative;
		value_string = parseInteger(value_string, &value, &is_negative, 32,
									encoding);
		if (value_string == NULL)
		{
			break;
		}
		if (value > 0x7fffffff)
		{
			llwarns << "value outside of valid range." << llendl;
			break;
		}
		array[i] = is_negative ? -S32(value) : S32(value);
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getUnsignedValue(U32 expected_length, U32* array,
								Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept bytes or integers
	if (mType != TYPE_INTEGER && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i = 0;
	// Int type
	for (i = 0; i < expected_length; ++i)
	{
		U64 value;
		bool is_negative;
		value_string = parseInteger(value_string, &value, &is_negative, 32,
									encoding);
		if (value_string == NULL)
		{
			break;
		}
		if (is_negative || value > 0xffffffff)
		{
			llwarns << "value outside of valid range." << llendl;
			break;
		}
		array[i] = U32(value);
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getLongValue(U32 expected_length, U64* array, Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept bytes or integers
	if (mType != TYPE_INTEGER && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i = 0;
	// Int type
	for (i = 0; i < expected_length; ++i)
	{
		U64 value;
		bool is_negative;
		value_string = parseInteger(value_string, &value, &is_negative, 64,
									encoding);
		if (value_string == NULL)
		{
			break;
		}
		if (is_negative)
		{
			llwarns << "value outside of valid range." << llendl;
			break;
		}
		array[i] = value;
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getFloatValue(U32 expected_length, F32* array,
							 Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept only floats or doubles
	if (mType != TYPE_FLOAT && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i;
	for (i = 0; i < expected_length; ++i)
	{
		F64 value;
		value_string = parseFloat(value_string, &value, 32, encoding);
		if (value_string == NULL)
		{
			break;
		}
		array[i] = F32(value);
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getDoubleValue(U32 expected_length, F64* array,
							  Encoding encoding)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type - accept only floats or doubles
	if (mType != TYPE_FLOAT && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	if (encoding == ENCODING_DEFAULT)
	{
		encoding = mEncoding;
	}

	const char* value_string = mValue.c_str();

	U32 i;
	for (i = 0; i < expected_length; ++i)
	{
		F64 value;
		value_string = parseFloat(value_string, &value, 64, encoding);
		if (value_string == NULL)
		{
			break;
		}
		array[i] = value;
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

	// Can always return any value as a string
U32 LLXMLNode::getStringValue(U32 expected_length, std::string* array)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	if (mLength > 0 && mLength != expected_length)
	{
		llwarns << "asked for " << expected_length
				<< " elements, while node has " << mLength << llendl;
		return 0;
	}

	U32 num_returned_strings = 0;

	// Array of strings is whitespace-separated
	const std::string sep(" \n\t");

	size_t n = 0;
	size_t m = 0;
	while (true)
	{
		if (num_returned_strings >= expected_length)
		{
			break;
		}
		n = mValue.find_first_not_of(sep, m);
		m = mValue.find_first_of(sep, n);
		if (m == std::string::npos)
		{
			break;
		}
		array[num_returned_strings++] = mValue.substr(n,m-n);
	}
	if (n != std::string::npos && num_returned_strings < expected_length)
	{
		array[num_returned_strings++] = mValue.substr(n);
	}

	if (num_returned_strings != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << num_returned_strings
							 << LL_ENDL;
	}

	return num_returned_strings;
}

U32 LLXMLNode::getUUIDValue(U32 expected_length, LLUUID* array)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type
	if (mType != TYPE_UUID && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	const char* value_string = mValue.c_str();

	U32 i;
	for (i = 0; i < expected_length; ++i)
	{
		LLUUID uuid_value;
		value_string = skipWhitespace(value_string);

		if (strlen(value_string) < UUID_STR_LENGTH - 1)
		{
			break;
		}
		char uuid_string[UUID_STR_LENGTH];
		memcpy(uuid_string, value_string, UUID_STR_LENGTH - 1);
		uuid_string[(UUID_STR_LENGTH - 1)] = 0;

		if (!LLUUID::parseUUID(std::string(uuid_string), &uuid_value))
		{
			break;
		}
		value_string = &value_string[(UUID_STR_LENGTH - 1)];
		array[i] = uuid_value;
	}

	if (i != expected_length)
	{
		LL_DEBUGS("XMLNode") << "failed for node named '" << mName->mString
							 << "'. Expected " << expected_length
							 << " but only found " << i << LL_ENDL;
	}

	return i;
}

U32 LLXMLNode::getNodeRefValue(U32 expected_length, LLXMLNode** array)
{
	if (!array)
	{
		llwarns << "NULL array pointer passed !" << llendl;
		llassert(false);
		return 0;
	}

	// Check type
	if (mType != TYPE_NODEREF && mType != TYPE_UNKNOWN)
	{
		return 0;
	}

	std::string *string_array = new std::string[expected_length];

	U32 num_strings = getStringValue(expected_length, string_array);

	U32 num_returned_refs = 0;

	LLXMLNodePtr root = getRoot();
	for (U32 strnum = 0; strnum < num_strings; ++strnum)
	{
		LLXMLNodeList node_list;
		root->findID(string_array[strnum], node_list);
		if (node_list.empty())
		{
			llwarns << "XML: Could not find node ID: " << string_array[strnum]
					<< llendl;
		}
		else if (node_list.size() > 1)
		{
			llwarns << "XML: Node ID not unique: " << string_array[strnum]
					<< llendl;
		}
		else
		{
			LLXMLNodeList::const_iterator list_itr = node_list.begin();
			if (list_itr != node_list.end())
			{
				LLXMLNode* child = (*list_itr).second;

				array[num_returned_refs++] = child;
			}
		}
	}

	delete[] string_array;

	return num_returned_refs;
}

void LLXMLNode::setBoolValue(U32 length, const bool* array)
{
	if (!length || !array) return;

	std::string new_value;
	for (U32 pos = 0; pos < length; ++pos)
	{
		if (pos > 0)
		{
			new_value = llformat("%s %s", new_value.c_str(),
								 array[pos] ? "true" : "false");
		}
		else
		{
			new_value = array[pos] ? "true" : "false";
		}
	}

	mValue = new_value;
	mEncoding = ENCODING_DEFAULT;
	mLength = length;
	mType = TYPE_BOOLEAN;
}

void LLXMLNode::setByteValue(U32 length, const U8* const array,
							 Encoding encoding)
{
	if (!length || !array) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(llformat(" %u", array[pos]));
			}
			else
			{
				new_value = llformat("%u", array[pos]);
			}
		}
	}
	if (encoding == ENCODING_HEX)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0 && pos % 16 == 0)
			{
				new_value.append(llformat(" %02X", array[pos]));
			}
			else
			{
				new_value.append(llformat("%02X", array[pos]));
			}
		}
	}
	// *TODO: handle Base32

	mValue = new_value;
	mEncoding = encoding;
	mLength = length;
	mType = TYPE_INTEGER;
	mPrecision = 8;
}

void LLXMLNode::setIntValue(U32 length, const S32* array, Encoding encoding)
{
	if (!length) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(llformat(" %d", array[pos]));
			}
			else
			{
				new_value = llformat("%d", array[pos]);
			}
		}
		mValue = new_value;
	}
	else if (encoding == ENCODING_HEX)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0 && pos % 16 == 0)
			{
				new_value.append(llformat(" %08X", ((U32*)array)[pos]));
			}
			else
			{
				new_value.append(llformat("%08X", ((U32*)array)[pos]));
			}
		}
		mValue = new_value;
	}
	else
	{
		mValue = new_value;
	}
	// *TODO: handle Base32

	mEncoding = encoding;
	mLength = length;
	mType = TYPE_INTEGER;
	mPrecision = 32;
}

void LLXMLNode::setUnsignedValue(U32 length, const U32* array, Encoding encoding)
{
	if (!length) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(llformat(" %u", array[pos]));
			}
			else
			{
				new_value = llformat("%u", array[pos]);
			}
		}
	}
	if (encoding == ENCODING_HEX)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0 && pos % 16 == 0)
			{
				new_value.append(llformat(" %08X", array[pos]));
			}
			else
			{
				new_value.append(llformat("%08X", array[pos]));
			}
		}
		mValue = new_value;
	}
	// *TODO: handle Base32

	mValue = new_value;
	mEncoding = encoding;
	mLength = length;
	mType = TYPE_INTEGER;
	mPrecision = 32;
}

#if LL_WINDOWS
#define PU64 "I64u"
#else
#define PU64 "llu"
#endif

void LLXMLNode::setLongValue(U32 length, const U64* array, Encoding encoding)
{
	if (!length) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(llformat(" %" PU64, array[pos]));
			}
			else
			{
				new_value = llformat("%" PU64, array[pos]);
			}
		}
		mValue = new_value;
	}
	if (encoding == ENCODING_HEX)
	{
		for (U32 pos = 0; pos < length; ++pos)
		{
			U32 upper_32 = U32(array[pos]>>32);
			U32 lower_32 = U32(array[pos]&0xffffffff);
			if (pos > 0 && pos % 8 == 0)
			{
				new_value.append(llformat(" %08X%08X", upper_32, lower_32));
			}
			else
			{
				new_value.append(llformat("%08X%08X", upper_32, lower_32));
			}
		}
		mValue = new_value;
	}
	else
	{
		mValue = new_value;
	}
	// *TODO: handle Base32

	mEncoding = encoding;
	mLength = length;
	mType = TYPE_INTEGER;
	mPrecision = 64;
}

void LLXMLNode::setFloatValue(U32 length, const F32* array, Encoding encoding,
							  U32 precision)
{
	if (!length) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		std::string format_string;
		if (precision > 0)
		{
			if (precision > 25)
			{
				precision = 25;
			}
			format_string = llformat("%%.%dg", precision);
		}
		else
		{
			format_string = llformat("%%g");
		}

		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(" ");
				new_value.append(llformat(format_string.c_str(), array[pos]));
			}
			else
			{
				new_value.assign(llformat(format_string.c_str(), array[pos]));
			}
		}
		mValue = new_value;
	}
	else if (encoding == ENCODING_HEX)
	{
		U32* byte_array = (U32*)array;
		setUnsignedValue(length, byte_array, ENCODING_HEX);
	}
	else
	{
		mValue = new_value;
	}

	mEncoding = encoding;
	mLength = length;
	mType = TYPE_FLOAT;
	mPrecision = 32;
}

void LLXMLNode::setDoubleValue(U32 length, const F64* array, Encoding encoding,
							   U32 precision)
{
	if (!length) return;

	std::string new_value;
	if (encoding == ENCODING_DEFAULT || encoding == ENCODING_DECIMAL)
	{
		std::string format_string;
		if (precision > 0)
		{
			if (precision > 25)
			{
				precision = 25;
			}
			format_string = llformat("%%.%dg", precision);
		}
		else
		{
			format_string = llformat("%%g");
		}
		for (U32 pos = 0; pos < length; ++pos)
		{
			if (pos > 0)
			{
				new_value.append(" ");
				new_value.append(llformat(format_string.c_str(), array[pos]));
			}
			else
			{
				new_value.assign(llformat(format_string.c_str(), array[pos]));
			}
		}
		mValue = new_value;
	}
	if (encoding == ENCODING_HEX)
	{
		U64* byte_array = (U64*)array;
		setLongValue(length, byte_array, ENCODING_HEX);
	}
	else
	{
		mValue = new_value;
	}
	// *TODO: handle Base32

	mEncoding = encoding;
	mLength = length;
	mType = TYPE_FLOAT;
	mPrecision = 64;
}

// static
std::string LLXMLNode::escapeXML(const std::string& xml)
{
	std::string out;
	for (size_t i = 0; i < xml.size(); ++i)
	{
		char c = xml[i];
		switch (c)
		{
			case '"':	out.append("&quot;");	break;
			case '\'':	out.append("&apos;");	break;
			case '&':	out.append("&amp;");	break;
			case '<':	out.append("&lt;");		break;
			case '>':	out.append("&gt;");		break;
			default:	out.push_back(c);		break;
		}
	}
	return out;
}

void LLXMLNode::setStringValue(U32 length, const std::string* strings)
{
	if (!length) return;

	std::string new_value;
	for (U32 pos = 0; pos < length; ++pos)
	{
		// *NOTE: Do not escape strings here: do it on output
		new_value.append(strings[pos]);
		if (pos < length - 1)
		{
			new_value.append(" ");
		}
	}

	mValue = new_value;
	mEncoding = ENCODING_DEFAULT;
	mLength = length;
	mType = TYPE_STRING;
}

void LLXMLNode::setUUIDValue(U32 length, const LLUUID* array)
{
	if (!length) return;

	std::string new_value;
	for (U32 pos = 0; pos < length; ++pos)
	{
		new_value.append(array[pos].asString());
		if (pos < length - 1)
		{
			new_value.append(" ");
		}
	}

	mValue = new_value;
	mEncoding = ENCODING_DEFAULT;
	mLength = length;
	mType = TYPE_UUID;
}

void LLXMLNode::setNodeRefValue(U32 length, const LLXMLNode** array)
{
	if (!length) return;

	std::string new_value;
	for (U32 pos = 0; pos < length; ++pos)
	{
		if (array[pos]->mID != "")
		{
			new_value.append(array[pos]->mID);
		}
		else
		{
			new_value.append("(null)");
		}
		if (pos < length - 1)
		{
			new_value.append(" ");
		}
	}

	mValue = new_value;
	mEncoding = ENCODING_DEFAULT;
	mLength = length;
	mType = TYPE_NODEREF;
}

void LLXMLNode::setValue(const std::string& value)
{
	if (mType == TYPE_CONTAINER)
	{
		mType = TYPE_UNKNOWN;
	}
	mValue = value;
}

void LLXMLNode::findDefault(LLXMLNode* defaults_list)
{
	if (defaults_list)
	{
		LLXMLNodeList children;
		defaults_list->getChildren(mName->mString, children);

		for (LLXMLNodeList::const_iterator it = children.begin(),
										   end = children.end();
			 it != end; ++it)
		{
			LLXMLNode* child = it->second;
			if (child && child->mVersionMajor == mVersionMajor &&
				child->mVersionMinor == mVersionMinor)
			{
				mDefault = child;
				return;
			}
		}
	}
	mDefault = NULL;
}

bool LLXMLNode::deleteChildren(const std::string& name)
{
	U32 removed_count = 0;
	LLXMLNodeList node_list;
	findName(name, node_list);
	if (!node_list.empty())
	{
		// *TODO: use multimap::find()
		// *TODO: need to watch out for invalid iterators
		for (LLXMLNodeList::const_iterator it = node_list.begin(),
										   end = node_list.end();
			 it != end; ++it)
		{
			LLXMLNode* child = it->second;
			if (deleteChild(child))
			{
				++removed_count;
			}
		}
	}
	return removed_count > 0;
}

bool LLXMLNode::deleteChildren(LLStringTableEntry* name)
{
	U32 removed_count = 0;
	LLXMLNodeList node_list;
	findName(name, node_list);
	if (!node_list.empty())
	{
		// *TODO: use multimap::find()
		// *TODO: need to watch out for invalid iterators
		LLXMLNodeList::iterator it;
		for (LLXMLNodeList::iterator it = node_list.begin(),
									 end = node_list.end();
			 it != end; ++it)
		{
			LLXMLNode* child = it->second;
			if (deleteChild(child))
			{
				++removed_count;
			}
		}
	}
	return removed_count > 0;
}

void LLXMLNode::setAttributes(LLXMLNode::ValueType type, U32 precision,
							  LLXMLNode::Encoding encoding, U32 length)
{
	mType = type;
	mEncoding = encoding;
	mPrecision = precision;
	mLength = length;
}

void LLXMLNode::setName(const std::string& name)
{
	setName(gStringTable.addStringEntry(name));
}

void LLXMLNode::setName(LLStringTableEntry* name)
{
	LLXMLNode* old_parent = mParent;
	if (mParent)
	{
		// We need to remove and re-add to the parent so that the multimap key
		// agrees with this node's name
		mParent->removeChild(this);
	}
	mName = name;
	if (old_parent)
	{
		old_parent->addChild(this);
	}
}

LLXMLNodePtr LLXMLNode::getFirstChild() const
{
	if (mChildren.isNull()) return NULL;
	LLXMLNodePtr ret = mChildren->head;
	return ret;
}

LLXMLNodePtr LLXMLNode::getNextSibling() const
{
	LLXMLNodePtr ret = mNext;
	return ret;
}

std::string LLXMLNode::getSanitizedValue() const
{
	if (mIsAttribute)
	{
		return getValue();
	}
	else
	{
		return getTextContents();
	}
}

std::string LLXMLNode::getTextContents() const
{
	std::string msg;
	std::string contents = mValue;
	size_t n = contents.find_first_not_of(" \t\n");
	if (n != std::string::npos && contents[n] == '\"')
	{
		// Case 1: node has quoted text
		S32 num_lines = 0;
		while (true)
		{
			// mContents[n] == '"'
			size_t t = ++n;
			size_t m = 0;
			// fix-up escaped characters
			while (true)
			{
				m = contents.find_first_of("\\\"", t); // find first \ or "
				if (m == std::string::npos || contents[m] == '\"')
				{
					break;
				}
				contents.erase(m, 1);
				t = m + 1;
			}
			if (m == std::string::npos)
			{
				break;
			}
			// mContents[m] == '"'
			++num_lines;
			msg += contents.substr(n, m - n) + "\n";
			n = contents.find_first_of("\"", m + 1);
			if (n == std::string::npos)
			{
				if (num_lines == 1)
				{
					msg.erase(msg.size() - 1); // remove "\n" if only one line
				}
				break;
			}
		}
	}
	else
	{
		// Case 2: node has embedded text (beginning and trailing white space
		// trimmed)
		size_t start = mValue.find_first_not_of(" \t\n");
		if (start != mValue.npos)
		{
			size_t end = mValue.find_last_not_of(" \t\n");
			if (end != mValue.npos)
			{
				msg = mValue.substr(start, end + 1 - start);
			}
			else
			{
				msg = mValue.substr(start);
			}
		}
		// Convert any internal CR to LF
		msg = utf8str_removeCRLF(msg);
	}
	return msg;
}
