/**
 * @file llxmlnode.h
 * @brief LLXMLNode definition
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLXMLNODE_H
#define LL_LLXMLNODE_H

#include <map>

#include "expat.h"

#include "indra_constants.h"
#include "llfile.h"
#include "llpointer.h"
#include "llpreprocessor.h"
#include "llrefcount.h"
#include "llstring.h"
#include "llstringtable.h"
#include "lluuid.h"

class LLColor4;
class LLColor4U;
class LLQuaternion;
class LLVector3;
class LLVector3d;

struct CompareAttributes
{
	LL_INLINE bool operator()(const LLStringTableEntry* const lhs,
							  const LLStringTableEntry* const rhs) const
	{
		if (lhs == NULL)
		{
			return true;
		}
		if (rhs == NULL)
		{
			return false;
		}
		return strcmp(lhs->mString, rhs->mString) < 0;
	}
};

// Defines a simple node hierarchy for reading and writing task objects

class LLXMLNode;
typedef LLPointer<LLXMLNode> LLXMLNodePtr;
typedef std::multimap<std::string, LLXMLNodePtr > LLXMLNodeList;
typedef std::multimap<const LLStringTableEntry*, LLXMLNodePtr > LLXMLChildList;
typedef std::map<const LLStringTableEntry*, LLXMLNodePtr, CompareAttributes> LLXMLAttribList;

class LLColor4;
class LLColor4U;
class LLQuaternion;
class LLVector3;
class LLVector3d;
class LLVector4;
class LLVector4U;

struct LLXMLChildren : public LLThreadSafeRefCount
{
	LLXMLChildList	map;	// Map of children names->pointers
	LLXMLNodePtr	head;	// Head of the double-linked list
	LLXMLNodePtr	tail;	// Tail of the double-linked list
};

typedef LLPointer<LLXMLChildren> LLXMLChildrenPtr;

class LLXMLNode : public LLThreadSafeRefCount
{
protected:
	LOG_CLASS(LLXMLNode);

public:
	enum ValueType
	{
		TYPE_CONTAINER,	// A node which contains nodes
		TYPE_UNKNOWN,	// A node loaded from file without a specified type
		TYPE_BOOLEAN,	// "true" or "false"
		TYPE_INTEGER,	// Any integer type: U8, U32, S32, U64, etc.
		TYPE_FLOAT,		// Any floating point type: F32, F64
		TYPE_STRING,	// A string
		TYPE_UUID,		// An UUID
		TYPE_NODEREF,	// The ID of another node in the hierarchy to reference
	};

	enum Encoding
	{
		ENCODING_DEFAULT = 0,
		ENCODING_DECIMAL,
		ENCODING_HEX,
		// ENCODING_BASE32, // Not implemented yet
	};

protected:
	~LLXMLNode();

public:
	LLXMLNode();
	LLXMLNode(const char* name, bool is_attribute);
	LLXMLNode(LLStringTableEntry* name, bool is_attribute);
	LLXMLNode(const LLXMLNode& rhs);
	LLXMLNodePtr deepCopy();

	LL_INLINE bool isNull()									{ return mName == NULL; }


	bool deleteChild(LLXMLNode* child);
    void addChild(LLXMLNodePtr new_child,
				  LLXMLNodePtr after_child = LLXMLNodePtr(NULL));
    void setParent(LLXMLNodePtr new_parent); // reparent if necessary

    // Serialization
	static bool parseFile(const std::string& filename, LLXMLNodePtr& node,
						  LLXMLNode* defaults_tree);
	static bool parseBuffer(U8* buffer, U32 length, LLXMLNodePtr& node,
							LLXMLNode* defaults);
	static bool parseStream(std::istream& str, LLXMLNodePtr& node,
							LLXMLNode* defaults);
	static bool updateNode(LLXMLNodePtr& node, LLXMLNodePtr& update_node);
	static LLXMLNodePtr replaceNode(LLXMLNodePtr node,
									LLXMLNodePtr replacement_node);

	static bool getLayeredXMLNode(LLXMLNodePtr& root,
								  const std::vector<std::string>& paths);

	// Write standard XML file header:
	// <?xml version="1.0" encoding="utf-8" standalone="yes" ?>
	static void writeHeaderToFile(LLFILE* out_file);

	// Write XML to file with one attribute per line.
	// XML escapes values as they are written.
    void writeToFile(LLFILE* out_file,
					 const std::string& indent = std::string(),
					 bool use_type_decorations = true);
    void writeToOstream(std::ostream& output_stream,
						const std::string& indent = std::string(),
						bool use_type_decorations = true);

    // Utility
    void findName(const std::string& name, LLXMLNodeList& results);
    void findName(LLStringTableEntry* name, LLXMLNodeList& results);
    void findID(const std::string& id, LLXMLNodeList& results);


    virtual LLXMLNodePtr createChild(const char* name, bool is_attribute);
    virtual LLXMLNodePtr createChild(LLStringTableEntry* name,
									 bool is_attribute);


    // Getters
    U32 getBoolValue(U32 expected_length, bool* array);
     U32 getByteValue(U32 expected_length, U8* array,
					 Encoding encoding = ENCODING_DEFAULT);
    U32 getIntValue(U32 expected_length, S32* array,
					Encoding encoding = ENCODING_DEFAULT);
    U32 getUnsignedValue(U32 expected_length, U32* array,
						 Encoding encoding = ENCODING_DEFAULT);
    U32 getLongValue(U32 expected_length, U64* array,
					 Encoding encoding = ENCODING_DEFAULT);
    U32 getFloatValue(U32 expected_length, F32* array,
					  Encoding encoding = ENCODING_DEFAULT);
    U32 getDoubleValue(U32 expected_length, F64* array,
					   Encoding encoding = ENCODING_DEFAULT);
    U32 getStringValue(U32 expected_length, std::string* array);
    U32 getUUIDValue(U32 expected_length, LLUUID* array);
    U32 getNodeRefValue(U32 expected_length, LLXMLNode** array);

	bool hasAttribute(const char* name);

	bool getAttributeBool(const char* name, bool& value);
	bool getAttributeU8(const char* name, U8& value);
	bool getAttributeS8(const char* name, S8& value);
	bool getAttributeU16(const char* name, U16& value);
	bool getAttributeS16(const char* name, S16& value);
	bool getAttributeU32(const char* name, U32& value);
	bool getAttributeS32(const char* name, S32& value);
	bool getAttributeF32(const char* name, F32& value);
	bool getAttributeF64(const char* name, F64& value);
	bool getAttributeColor(const char* name, LLColor4& value);
	bool getAttributeColor4(const char* name, LLColor4& value);
	bool getAttributeColor4U(const char* name, LLColor4U& value);
	bool getAttributeVector3(const char* name, LLVector3& value);
	bool getAttributeVector3d(const char* name, LLVector3d& value);
	bool getAttributeQuat(const char* name, LLQuaternion& value);
	bool getAttributeUUID(const char* name, LLUUID& value);
	bool getAttributeString(const char* name, std::string& value);

    LL_INLINE const ValueType& getType() const				{ return mType; }
    LL_INLINE U32 getLength() const							{ return mLength; }
    LL_INLINE U32 getPrecision() const						{ return mPrecision; }
    LL_INLINE const std::string& getValue() const			{ return mValue; }
	std::string getSanitizedValue() const;
	std::string getTextContents() const;
    LL_INLINE const LLStringTableEntry* getName() const		{ return mName; }
	LL_INLINE bool hasName(const char* name) const			{ return mName == gStringTable.checkStringEntry(name); }
	LL_INLINE bool hasName(const std::string& name) const	{ return mName == gStringTable.checkStringEntry(name.c_str()); }
    LL_INLINE const std::string& getID() const				{ return mID; }

    LL_INLINE U32 getChildCount() const						{ return mChildren.notNull() ? (U32)mChildren->map.size() : 0U; }
    // getChild returns a Null LLXMLNode (not a NULL pointer) if there is no such child.
    // This child has no value so any getTYPEValue() calls on it will return 0.
    bool getChild(const char* name, LLXMLNodePtr& node, bool use_default_if_missing = true);
    bool getChild(const LLStringTableEntry* name, LLXMLNodePtr& node, bool use_default_if_missing = true);
    void getChildren(const char* name, LLXMLNodeList &children, bool use_default_if_missing = true) const;
    void getChildren(const LLStringTableEntry* name, LLXMLNodeList& children, bool use_default_if_missing = true) const;

	// Recursively finds all children at any level matching name
	void getDescendants(const LLStringTableEntry* name, LLXMLNodeList &children) const;

	bool getAttribute(const char* name, LLXMLNodePtr& node, bool use_default_if_missing = true);
	bool getAttribute(const LLStringTableEntry* name, LLXMLNodePtr& node, bool use_default_if_missing = true);

	LL_INLINE S32 getLineNumber()							{ return mLineNumber; }

	// The following skip over attributes
	LLXMLNodePtr getFirstChild() const;
	LLXMLNodePtr getNextSibling() const;

    LLXMLNodePtr getRoot();

	// Setters

	bool setAttributeString(const char* attr, const std::string& value);

	LL_INLINE void setBoolValue(bool value)					{ setBoolValue(1, &value); }

	LL_INLINE void setByteValue(U8 value, Encoding encoding = ENCODING_DEFAULT)
	{
		setByteValue(1, &value, encoding);
	}

	LL_INLINE void setIntValue(S32 value, Encoding encoding = ENCODING_DEFAULT)
	{
		setIntValue(1, &value, encoding);
	}

	LL_INLINE void setUnsignedValue(U32 value,
									Encoding encoding = ENCODING_DEFAULT)
	{
		setUnsignedValue(1, &value, encoding);
	}

	LL_INLINE void setLongValue(U64 value,
								Encoding encoding = ENCODING_DEFAULT)
	{
		setLongValue(1, &value, encoding);
	}

	LL_INLINE void setFloatValue(F32 value,
								 Encoding encoding = ENCODING_DEFAULT,
								 U32 precision = 0)
	{
		setFloatValue(1, &value, encoding);
	}

	LL_INLINE void setDoubleValue(F64 value,
								  Encoding encoding = ENCODING_DEFAULT,
								  U32 precision = 0)
	{
		setDoubleValue(1, &value, encoding);
	}

	LL_INLINE void setStringValue(const std::string& value)	{ setStringValue(1, &value); }
	LL_INLINE void setUUIDValue(const LLUUID value)			{ setUUIDValue(1, &value); }
	LL_INLINE void setNodeRefValue(const LLXMLNode* value)	{ setNodeRefValue(1, &value); }

	void setBoolValue(U32 length, const bool* array);
	void setByteValue(U32 length, const U8* array,
					  Encoding encoding = ENCODING_DEFAULT);
	void setIntValue(U32 length, const S32* array,
					 Encoding encoding = ENCODING_DEFAULT);
	void setUnsignedValue(U32 length, const U32* array,
						  Encoding encoding = ENCODING_DEFAULT);
	void setLongValue(U32 length, const U64* array,
					  Encoding encoding = ENCODING_DEFAULT);
	void setFloatValue(U32 length, const F32* array,
					   Encoding encoding = ENCODING_DEFAULT,
					   U32 precision = 0);
	void setDoubleValue(U32 length, const F64* array,
						Encoding encoding = ENCODING_DEFAULT,
						U32 precision = 0);
	void setStringValue(U32 length, const std::string* array);
	void setUUIDValue(U32 length, const LLUUID* array);
	void setNodeRefValue(U32 length, const LLXMLNode** array);
	void setValue(const std::string& value);
	void setName(const std::string& name);
	void setName(LLStringTableEntry* name);

	LL_INLINE void setLineNumber(S32 line_number)			{ mLineNumber = line_number; }

	// Escapes " (quot) ' (apos) & (amp) < (lt) > (gt)
	static std::string escapeXML(const std::string& xml);

	// Set the default node corresponding to this default node
	LL_INLINE void setDefault(LLXMLNode* default_node)		{ mDefault = default_node; }

	// Find the node within defaults_list which corresponds to this node
	void findDefault(LLXMLNode *defaults_list);

	void updateDefault();

	// Delete any child nodes that are not among the tree's children, recursive
	void scrubToTree(LLXMLNode *tree);

	bool deleteChildren(const std::string& name);
	bool deleteChildren(LLStringTableEntry* name);
	void setAttributes(ValueType type, U32 precision, Encoding encoding,
					   U32 length);
#if 0 // Unused
	LL_INLINE void appendValue(const std::string& value)	{ mValue.append(value); }
#endif

protected:
	bool removeChild(LLXMLNode* child);
	bool isFullyDefault();

	static const char* skipWhitespace(const char* str);
	static const char* skipNonWhitespace(const char* str);
	static const char* parseInteger(const char* str, U64* dest,
									bool* is_negative, U32 precision,
									Encoding encoding);
	static const char* parseFloat(const char* str, F64* dest, U32 precision,
								  Encoding encoding);

protected:
	LLStringTableEntry*	mName;			// The name of this node

	LLXMLNodePtr		mDefault;		// Mirror node in the default tree

	// The value of this node (use getters/setters only)
	// Values are not XML-escaped in memory
	// They may contain " (quot) ' (apos) & (amp) < (lt) > (gt)
	std::string			mValue;

public:
	std::string			mID;			// The ID attribute of this node

	// Temporary pointer while loading
	XML_Parser*			mParser;

	LLXMLNode*			mParent;		// The parent node
	LLXMLChildrenPtr	mChildren;		// The child nodes
	LLXMLAttribList		mAttributes;	// The attribute nodes
	LLXMLNodePtr		mPrev;			// Double-linked list previous node
	LLXMLNodePtr		mNext;			// Double-linked list next node

	U32					mVersionMajor;	// Version of this tag to use
	U32					mVersionMinor;
	// If length != 0 then only return arrays of this length
	U32					mLength;
	U32					mPrecision;		// The number of BITS per array item
	ValueType			mType;			// The value type
	Encoding			mEncoding;		// The value encoding
	// Line number in source file, if applicable
	S32					mLineNumber;

	// Flag is only used for output formatting
	bool				mIsAttribute;

	static bool			sStripEscapedStrings;
	static bool			sStripWhitespaceValues;
};

#endif // LL_LLXMLNODE
