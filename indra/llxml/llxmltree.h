/**
 * @file llxmltree.h
 * @author Aaron Yonas, Richard Nelson
 * @brief LLXmlTree class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLXMLTREE_H
#define LL_LLXMLTREE_H

#include <map>
#include <vector>

#include "llstring.h"
#include "llstringtable.h"
#include "llxmlparser.h"

class LLColor4;
class LLColor4U;
class LLQuaternion;
class LLUUID;
class LLVector3;
class LLVector3d;
class LLXmlTreeNode;
class LLXmlTreeParser;

class LLXmlTree
{
	friend class LLXmlTreeNode;

protected:
	LOG_CLASS(LLXmlTree);

public:
	LLXmlTree();
	virtual ~LLXmlTree();
	void cleanup();

	virtual bool parseFile(const std::string& path, bool keep_contents = true);

	LL_INLINE LLXmlTreeNode* getRoot()				{ return mRoot; }

	void dump();
	void dumpNode(LLXmlTreeNode* node, const std::string& prefix);

	LL_INLINE static LLStdStringHandle addAttributeString(const std::string& name)
	{
		return sAttributeKeys.addString(name);
	}

public:
	// global
	static LLStdStringTable sAttributeKeys;

protected:
	LLXmlTreeNode* mRoot;

	// local
	LLStdStringTable mNodeNames;
};

class LLXmlTreeNode
{
	friend class LLXmlTree;
	friend class LLXmlTreeParser;

protected:
	LOG_CLASS(LLXmlTreeNode);

	// Protected since nodes are only created and destroyed by friend classes
	// and other LLXmlTreeNodes
	LLXmlTreeNode(const char* name, LLXmlTreeNode* parent, LLXmlTree* tree);

public:
	virtual ~LLXmlTreeNode();

	LL_INLINE const std::string& getName()			{ return mName; }

	LL_INLINE bool hasName(const std::string& name)	{ return mName == name; }

	bool hasAttribute(const std::string& name);

	// Fast versions using handles
	bool getFastAttributeBool(LLStdStringHandle cannonical_name, bool& value);
	bool getFastAttributeU8(LLStdStringHandle cannonical_name, U8& value);
	bool getFastAttributeS8(LLStdStringHandle cannonical_name, S8& value);
	bool getFastAttributeU16(LLStdStringHandle cannonical_name, U16& value);
	bool getFastAttributeS16(LLStdStringHandle cannonical_name, S16& value);
	bool getFastAttributeU32(LLStdStringHandle cannonical_name, U32& value);
	bool getFastAttributeS32(LLStdStringHandle cannonical_name, S32& value);
	bool getFastAttributeF32(LLStdStringHandle cannonical_name, F32& value);
	bool getFastAttributeF64(LLStdStringHandle cannonical_name, F64& value);
	bool getFastAttributeColor(LLStdStringHandle cannonical_name,
							   LLColor4& value);
	bool getFastAttributeColor4(LLStdStringHandle cannonical_name,
								LLColor4& value);
	bool getFastAttributeColor4U(LLStdStringHandle cannonical_name,
								 LLColor4U& value);
	bool getFastAttributeVector3(LLStdStringHandle cannonical_name,
								 LLVector3& value);
	bool getFastAttributeVector3d(LLStdStringHandle cannonical_name,
								  LLVector3d& value);
	bool getFastAttributeQuat(LLStdStringHandle cannonical_name,
							  LLQuaternion& value);
	bool getFastAttributeUUID(LLStdStringHandle cannonical_name,
							  LLUUID& value);
	bool getFastAttributeString(LLStdStringHandle cannonical_name,
								std::string& value);

	// Normal versions find 'name' in LLXmlTree::sAttributeKeys then call fast
	// versions
	virtual bool getAttributeBool(const char* name, bool& value);
	virtual bool getAttributeU8(const char* name, U8& value);
	virtual bool getAttributeS8(const char* name, S8& value);
	virtual bool getAttributeU16(const char* name, U16& value);
	virtual bool getAttributeS16(const char* name, S16& value);
	virtual bool getAttributeU32(const char* name, U32& value);
	virtual bool getAttributeS32(const char* name, S32& value);
	virtual bool getAttributeF32(const char* name, F32& value);
	virtual bool getAttributeF64(const char* name, F64& value);
	virtual bool getAttributeColor(const char* name, LLColor4& value);
	virtual bool getAttributeColor4(const char* name, LLColor4& value);
	virtual bool getAttributeColor4U(const char* name, LLColor4U& value);
	virtual bool getAttributeVector3(const char* name, LLVector3& value);
	virtual bool getAttributeVector3d(const char* name, LLVector3d& value);
	virtual bool getAttributeQuat(const char* name, LLQuaternion& value);
	virtual bool getAttributeUUID(const char* name, LLUUID& value);
	virtual bool getAttributeString(const char* name, std::string& value);

	LL_INLINE const std::string& getContents()		{ return mContents; }
	std::string getTextContents();

	LL_INLINE LLXmlTreeNode* getParent()			{ return mParent; }
	LLXmlTreeNode* getFirstChild();
	LLXmlTreeNode* getNextChild();
	LL_INLINE S32 getChildCount()					{ return (S32)mChildren.size(); }
	// returns first child with name, NULL if none
	LLXmlTreeNode* getChildByName(const std::string& name);
	// returns next child with name, NULL if none
	LLXmlTreeNode* getNextNamedChild();

protected:
	LL_INLINE const std::string* getAttribute(LLStdStringHandle name)
	{
		attribute_map_t::iterator iter = mAttributes.find(name);
		return iter == mAttributes.end() ? NULL : iter->second;
	}

private:
	void addAttribute(const char* name, const std::string& value);
	void appendContents(const std::string& str);
	void addChild(LLXmlTreeNode* child);

	void dump(const std::string& prefix);

protected:
	typedef std::map<LLStdStringHandle, const std::string*> attribute_map_t;
	attribute_map_t			mAttributes;

private:
	std::string				mName;
	std::string				mContents;

	typedef std::vector<class LLXmlTreeNode*> child_vec_t;
	child_vec_t				mChildren;
	child_vec_t::iterator	mChildrenIter;

	typedef std::multimap<LLStdStringHandle, LLXmlTreeNode*> child_map_t;
	child_map_t				mChildMap;		// For fast name lookups
	child_map_t::iterator	mChildMapIter;
	child_map_t::iterator	mChildMapEndIter;

	LLXmlTreeNode*			mParent;
	LLXmlTree*				mTree;
};

class LLXmlTreeParser : public LLXmlParser
{
protected:
	LOG_CLASS(LLXmlTreeParser);

public:
	LLXmlTreeParser(LLXmlTree* tree);

	bool parseFile(const std::string& path, LLXmlTreeNode** root,
				   bool keep_contents);

protected:
	const std::string& tabs();

	void startElement(const char* name, const char** attributes) override;
	void endElement(const char* name) override;
	void characterData(const char* s, int len) override;
	void processingInstruction(const char* target, const char* data) override;

	void comment(const char* data) override;
	void startCdataSection() override;
	void endCdataSection() override;
	void defaultData(const char* s, int len) override;
	void unparsedEntityDecl(const char* entity_name, const char* base,
							const char* system_id, const char* public_id,
							const char* notation_name) override;

	// Template method pattern
	LLXmlTreeNode* createXmlTreeNode(const char* name, LLXmlTreeNode* parent);

protected:
	LLXmlTree*		mTree;
	LLXmlTreeNode*	mRoot;
	LLXmlTreeNode*  mCurrent;
	bool			mDump;	// Dump parse tree to llinfos as it is read.
	bool			mKeepContents;
};

#endif  // LL_LLXMLTREE_H
