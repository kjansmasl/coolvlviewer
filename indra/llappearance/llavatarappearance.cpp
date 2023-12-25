/**
 * @File llavatarappearance.cpp
 * @brief Implementation of LLAvatarAppearance class
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

#include "linden_common.h"

#include "boost/lexical_cast.hpp"
#include "boost/tokenizer.hpp"

#include "llavatarappearance.h"

#include "llavatarappearancedefines.h"
#include "llavatarjointmesh.h"
#include "lldir.h"
#include "llpolymesh.h"
#include "llpolymorph.h"
#include "llpolyskeletaldistortion.h"
#include "llstl.h"
#include "lltexglobalcolor.h"
#include "llwearabledata.h"
#include "imageids.h"

using namespace LLAvatarAppearanceDefines;

// Constants
const std::string AVATAR_DEFAULT_CHAR = "avatar_lad.xml";
const LLColor4 DUMMY_COLOR = LLColor4(0.5f, 0.5f, 0.5f, 1.f);

// Static variables
LLAvatarSkeletonInfo* LLAvatarAppearance::sAvatarSkeletonInfo = NULL;
LLAvatarAppearance::LLAvatarXmlInfo* LLAvatarAppearance::sAvatarXmlInfo = NULL;

//------------------------------------------------------------------------
// LLAvatarBoneInfo class
// Trans/Scale/Rot etc. info about each avatar bone. Used by
// LLVOAvatarSkeleton.
//------------------------------------------------------------------------

class LLAvatarBoneInfo
{
	friend class LLAvatarAppearance;
	friend class LLAvatarSkeletonInfo;

public:
	LLAvatarBoneInfo()
	:	mIsJoint(false)
	{
	}

	~LLAvatarBoneInfo()
	{
		std::for_each(mChildList.begin(), mChildList.end(), DeletePointer());
		mChildList.clear();
	}

	bool parseXml(LLXmlTreeNode* node);

private:
	LLVector3		mPos;
	LLVector3		mEnd;
	LLVector3		mRot;
	LLVector3		mScale;
	LLVector3		mPivot;
	std::string		mName;
	std::string		mSupport;
	std::string		mAliases;
	bool			mIsJoint;
	typedef std::vector<LLAvatarBoneInfo*> child_list_t;
	child_list_t	mChildList;
};

bool LLAvatarBoneInfo::parseXml(LLXmlTreeNode* node)
{
	if (node->hasName("bone"))
	{
		mIsJoint = true;
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		if (!node->getFastAttributeString(name_string, mName))
		{
			llwarns << "Bone without name" << llendl;
			return false;
		}

		static LLStdStringHandle aliases_string =
			LLXmlTree::addAttributeString("aliases");
		// Note: aliases are not required.
		node->getFastAttributeString(aliases_string, mAliases);
	}
	else if (node->hasName("collision_volume"))
	{
		mIsJoint = false;
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		if (!node->getFastAttributeString(name_string, mName))
		{
			mName = "Collision Volume";
		}
	}
	else
	{
		llwarns << "Invalid node " << node->getName() << llendl;
		return false;
	}

	static LLStdStringHandle pos_string = LLXmlTree::addAttributeString("pos");
	if (!node->getFastAttributeVector3(pos_string, mPos))
	{
		llwarns << "Bone without position" << llendl;
		return false;
	}

	static LLStdStringHandle rot_string = LLXmlTree::addAttributeString("rot");
	if (!node->getFastAttributeVector3(rot_string, mRot))
	{
		llwarns << "Bone without rotation" << llendl;
		return false;
	}

	static LLStdStringHandle scale_string =
		LLXmlTree::addAttributeString("scale");
	if (!node->getFastAttributeVector3(scale_string, mScale))
	{
		llwarns << "Bone without scale" << llendl;
		return false;
	}

	static LLStdStringHandle end_string = LLXmlTree::addAttributeString("end");
	if (!node->getFastAttributeVector3(end_string, mEnd))
	{
		llwarns << "Bone without end" << llendl;
		mEnd = LLVector3(0.0f, 0.0f, 0.0f);
	}

	static LLStdStringHandle support_string =
		LLXmlTree::addAttributeString("support");
	if (!node->getFastAttributeString(support_string, mSupport))
	{
		llwarns << "Bone without support" << llendl;
		mSupport = "base";
	}

	if (mIsJoint)
	{
		static LLStdStringHandle pivot_string =
			LLXmlTree::addAttributeString("pivot");
		if (!node->getFastAttributeVector3(pivot_string, mPivot))
		{
			llwarns << "Bone without pivot" << llendl;
			return false;
		}
	}

	// Parse children
	for (LLXmlTreeNode* child = node->getFirstChild(); child;
		 child = node->getNextChild())
	{
		LLAvatarBoneInfo* child_info = new LLAvatarBoneInfo;
		if (!child_info->parseXml(child))
		{
			delete child_info;
			return false;
		}
		mChildList.push_back(child_info);
	}
	return true;
}

//------------------------------------------------------------------------
// LLAvatarSkeletonInfo class
// Overall avatar skeleton
//------------------------------------------------------------------------

class LLAvatarSkeletonInfo
{
	friend class LLAvatarAppearance;

public:
	LLAvatarSkeletonInfo()
	:	mNumBones(0),
		mNumCollisionVolumes(0)
	{
	}

	~LLAvatarSkeletonInfo()
	{
		std::for_each(mBoneInfoList.begin(), mBoneInfoList.end(),
					  DeletePointer());
		mBoneInfoList.clear();
	}

	bool parseXml(LLXmlTreeNode* node);
	S32 getNumBones() const						{ return mNumBones; }
	S32 getNumCollisionVolumes() const			{ return mNumCollisionVolumes; }

private:
	S32										mNumBones;
	S32										mNumCollisionVolumes;

	typedef std::vector<LLAvatarBoneInfo*> bone_info_list_t;
	bone_info_list_t						mBoneInfoList;
};

bool LLAvatarSkeletonInfo::parseXml(LLXmlTreeNode* node)
{
	static LLStdStringHandle num_bones_string =
		LLXmlTree::addAttributeString("num_bones");
	if (!node->getFastAttributeS32(num_bones_string, mNumBones))
	{
		llwarns << "Couldn't find number of bones." << llendl;
		return false;
	}

	static LLStdStringHandle num_collision_volumes_string =
		LLXmlTree::addAttributeString("num_collision_volumes");
	node->getFastAttributeS32(num_collision_volumes_string,
							  mNumCollisionVolumes);

	LLXmlTreeNode* child;
	for (child = node->getFirstChild(); child; child = node->getNextChild())
	{
		LLAvatarBoneInfo* info = new LLAvatarBoneInfo;
		if (!info->parseXml(child))
		{
			delete info;
			llwarns << "Error parsing bone in skeleton file" << llendl;
			return false;
		}
		mBoneInfoList.push_back(info);
	}
	return true;
}

// Makes aliases for joint and pushes them to map.
void LLAvatarAppearance::makeJointAliases(LLAvatarBoneInfo* bone_info)
{
	if (!bone_info || !bone_info->mIsJoint)
	{
		return;
	}

	std::string bone_name = bone_info->mName;
	// Actual name is a valid alias; add it.
	mJointAliasMap[bone_name] = bone_name;

	std::string aliases = bone_info->mAliases;

	boost::char_separator<char> sep(" ");
	boost::tokenizer<boost::char_separator<char> > tok(aliases, sep);
	for (boost::tokenizer<boost::char_separator<char> >::iterator
			i = tok.begin(), end = tok.end();
		 i != end; ++i)
	{
		if (mJointAliasMap.find(*i) != mJointAliasMap.end())
		{
			llwarns << "Avatar skeleton joint alias \"" << *i
					<< "\" remapped from \"" << mJointAliasMap[*i]
					<< "\" to \"" << bone_name << "\"" << llendl;
		}
		mJointAliasMap[*i] = bone_name;
	}

	for (LLAvatarBoneInfo::child_list_t::const_iterator
			iter = bone_info->mChildList.begin(),
			end = bone_info->mChildList.end();
		 iter != end; ++iter)
	{
		makeJointAliases(*iter);
	}
}

const joint_alias_map_t& LLAvatarAppearance::getJointAliases()
{
	if (mJointAliasMap.empty())
	{
		for (LLAvatarSkeletonInfo::bone_info_list_t::const_iterator
				iter = sAvatarSkeletonInfo->mBoneInfoList.begin(),
				end = sAvatarSkeletonInfo->mBoneInfoList.end();
			iter != end; ++iter)
		{
			makeJointAliases(*iter);
		}
	}

	// Also accept the name with spaces substituted with underscores. This
	// gives a mechanism for referencing such joints in Collada files which do
	// not allow spaces.
	std::string underscored;
	for (LLAvatarXmlInfo::attachment_info_list_t::iterator
			it = sAvatarXmlInfo->mAttachmentInfoList.begin(),
			end = sAvatarXmlInfo->mAttachmentInfoList.end();
		 it != end; ++it)
	{
		LLAvatarXmlInfo::LLAvatarAttachmentInfo* info = *it;
		if (info)
		{
			const std::string& bone_name = info->mName;
			underscored = bone_name;
			LLStringUtil::replaceChar(underscored, ' ', '_');
			if (underscored != bone_name)
			{
				mJointAliasMap[underscored] = bone_name;
			}
		}
	}

	return mJointAliasMap;
}

LLJoint* LLAvatarAppearance::getSkeletonJoint(S32 num)
{
	return num >= 0 && num < (S32)mSkeleton.size() ? mSkeleton[num] : NULL;
}

//-----------------------------------------------------------------------------
// LLAvatarXmlInfo sub-class
//-----------------------------------------------------------------------------

LLAvatarAppearance::LLAvatarXmlInfo::LLAvatarXmlInfo()
:	mTexSkinColorInfo(NULL),
	mTexHairColorInfo(NULL),
	mTexEyeColorInfo(NULL)
{
}

LLAvatarAppearance::LLAvatarXmlInfo::~LLAvatarXmlInfo()
{
	std::for_each(mMeshInfoList.begin(), mMeshInfoList.end(), DeletePointer());
	mMeshInfoList.clear();
	std::for_each(mSkeletalDistortionInfoList.begin(),
				  mSkeletalDistortionInfoList.end(), DeletePointer());
	mSkeletalDistortionInfoList.clear();
	std::for_each(mAttachmentInfoList.begin(), mAttachmentInfoList.end(),
				  DeletePointer());
	mAttachmentInfoList.clear();
	delete_and_clear(mTexSkinColorInfo);
	delete_and_clear(mTexHairColorInfo);
	delete_and_clear(mTexEyeColorInfo);
	std::for_each(mLayerInfoList.begin(), mLayerInfoList.end(),
				  DeletePointer());
	mLayerInfoList.clear();
	std::for_each(mDriverInfoList.begin(), mDriverInfoList.end(),
				  DeletePointer());
	mDriverInfoList.clear();
	std::for_each(mMorphMaskInfoList.begin(), mMorphMaskInfoList.end(),
				  DeletePointer());
	mMorphMaskInfoList.clear();
}

bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlSkeletonNode(LLXmlTreeNode* root)
{
	LLXmlTreeNode* node = root->getChildByName("skeleton");
	if (!node)
	{
		llwarns << "avatar file: missing <skeleton>" << llendl;
		return false;
	}

	LLXmlTreeNode* child;

	// SKELETON DISTORTIONS
	for (child = node->getChildByName("param"); child;
		 child = node->getNextNamedChild())
	{
		if (!child->getChildByName("param_skeleton"))
		{
			if (child->getChildByName("param_morph"))
			{
				llwarns << "Cannot specify morph param in skeleton definition."
						<< llendl;
			}
			else
			{
				llwarns << "Unknown param type." << llendl;
			}
			return false;
		}

		LLPolySkeletalDistortionInfo* info = new LLPolySkeletalDistortionInfo;
		if (!info->parseXml(child))
		{
			delete info;
			return false;
		}

		mSkeletalDistortionInfoList.push_back(info);
	}

	// ATTACHMENT POINTS
	for (child = node->getChildByName("attachment_point"); child;
		 child = node->getNextNamedChild())
	{
		LLAvatarAttachmentInfo* info = new LLAvatarAttachmentInfo();

		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		if (!child->getFastAttributeString(name_string, info->mName))
		{
			llwarns << "No name supplied for attachment point." << llendl;
			delete info;
			return false;
		}

		static LLStdStringHandle joint_string =
			LLXmlTree::addAttributeString("joint");
		if (!child->getFastAttributeString(joint_string, info->mJointName))
		{
			llwarns << "No bone declared in attachment point " << info->mName
					<< llendl;
			delete info;
			return false;
		}
		info->mJointKey = LLJoint::getKey(info->mJointName);

		static LLStdStringHandle position_string =
			LLXmlTree::addAttributeString("position");
		if (child->getFastAttributeVector3(position_string, info->mPosition))
		{
			info->mHasPosition = true;
		}

		static LLStdStringHandle rotation_string =
			LLXmlTree::addAttributeString("rotation");
		if (child->getFastAttributeVector3(rotation_string, info->mRotationEuler))
		{
			info->mHasRotation = true;
		}

		static LLStdStringHandle group_string =
			LLXmlTree::addAttributeString("group");
		if (child->getFastAttributeS32(group_string, info->mGroup))
		{
			if (info->mGroup == -1)
				info->mGroup = -1111; // -1 = none parsed, < -1 = bad value
		}

		static LLStdStringHandle id_string =
			LLXmlTree::addAttributeString("id");
		if (!child->getFastAttributeS32(id_string, info->mAttachmentID))
		{
			llwarns << "No id supplied for attachment point " << info->mName
					<< llendl;
			delete info;
			return false;
		}

		static LLStdStringHandle slot_string =
			LLXmlTree::addAttributeString("pie_slice");
		child->getFastAttributeS32(slot_string, info->mPieMenuSlice);

		static LLStdStringHandle visible_in_first_person_string =
			LLXmlTree::addAttributeString("visible_in_first_person");
		child->getFastAttributeBool(visible_in_first_person_string,
									info->mVisibleFirstPerson);

		static LLStdStringHandle hud_attachment_string =
			LLXmlTree::addAttributeString("hud");
		child->getFastAttributeBool(hud_attachment_string,
									info->mIsHUDAttachment);

		mAttachmentInfoList.push_back(info);
	}

	return true;
}

// Parses <mesh> nodes from XML tree
bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlMeshNodes(LLXmlTreeNode* root)
{
	for (LLXmlTreeNode* node = root->getChildByName("mesh"); node;
		 node = root->getNextNamedChild())
	{
		LLAvatarMeshInfo* info = new LLAvatarMeshInfo;

		// attribute: type
		static LLStdStringHandle type_string =
			LLXmlTree::addAttributeString("type");
		if (!node->getFastAttributeString(type_string, info->mType))
		{
			llwarns << "Avatar file: <mesh> is missing type attribute. Ignoring element."
					<< llendl;
			delete info;
			return false;  // Ignore this element
		}

		static LLStdStringHandle lod_string =
			LLXmlTree::addAttributeString("lod");
		if (!node->getFastAttributeS32(lod_string, info->mLOD))
		{
			llwarns << "Avatar file: <mesh> is missing lod attribute. Ignoring element."
					<< llendl;
			delete info;
			return false;  // Ignore this element
		}

		static LLStdStringHandle filename_str =
			LLXmlTree::addAttributeString("file_name");
		if (!node->getFastAttributeString(filename_str, info->mMeshFileName))
		{
			llwarns << "Avatar file: <mesh> is missing file_name attribute. Ignoring: "
					<< info->mType << llendl;
			delete info;
			return false;  // Ignore this element
		}

		static LLStdStringHandle reference_string =
			LLXmlTree::addAttributeString("reference");
		node->getFastAttributeString(reference_string, info->mReferenceMeshName);

		// Attribute: min_pixel_area
		static LLStdStringHandle min_pixel_area_string =
			LLXmlTree::addAttributeString("min_pixel_area");
		static LLStdStringHandle min_pixel_width_string =
			LLXmlTree::addAttributeString("min_pixel_width");
		if (!node->getFastAttributeF32(min_pixel_area_string,
									   info->mMinPixelArea))
		{
			F32 min_pixel_area = 0.1f;
			if (node->getFastAttributeF32(min_pixel_width_string, min_pixel_area))
			{
				// this is square root of pixel area (sensible to use linear
				// space in defining lods)
				min_pixel_area = min_pixel_area * min_pixel_area;
			}
			info->mMinPixelArea = min_pixel_area;
		}

		// Parse visual params for this node only if we haven't already
		for (LLXmlTreeNode* child = node->getChildByName("param"); child;
			 child = node->getNextNamedChild())
		{
			if (!child->getChildByName("param_morph"))
			{
				if (child->getChildByName("param_skeleton"))
				{
					llwarns << "Cannot specify skeleton param in a mesh definition."
							<< llendl;
				}
				else
				{
					llwarns << "Unknown param type." << llendl;
				}
				return false;
			}

			LLPolyMorphTargetInfo* morphinfo = new LLPolyMorphTargetInfo();
			if (!morphinfo->parseXml(child))
			{
				delete morphinfo;
				delete info;
				return false;
			}
			bool shared = false;
			static LLStdStringHandle shared_string =
				LLXmlTree::addAttributeString("shared");
			child->getFastAttributeBool(shared_string, shared);

			info->mPolyMorphTargetInfoList.emplace_back(morphinfo, shared);
		}

		mMeshInfoList.push_back(info);
	}
	return true;
}

// Parses <global_color> nodes from XML tree
bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlColorNodes(LLXmlTreeNode* root)
{
	for (LLXmlTreeNode* color_node = root->getChildByName("global_color");
		 color_node; color_node = root->getNextNamedChild())
	{
		std::string global_color_name;
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		if (color_node->getFastAttributeString(name_string, global_color_name))
		{
			if (global_color_name == "skin_color")
			{
				if (mTexSkinColorInfo)
				{
					llwarns << "avatar file: multiple instances of skin_color"
							<< llendl;
					return false;
				}
				mTexSkinColorInfo = new LLTexGlobalColorInfo;
				if (!mTexSkinColorInfo->parseXml(color_node))
				{
					delete_and_clear(mTexSkinColorInfo);
					llwarns << "avatar file: mTexSkinColor->parseXml() failed"
							<< llendl;
					return false;
				}
			}
			else if (global_color_name == "hair_color")
			{
				if (mTexHairColorInfo)
				{
					llwarns << "avatar file: multiple instances of hair_color"
							<< llendl;
					return false;
				}
				mTexHairColorInfo = new LLTexGlobalColorInfo;
				if (!mTexHairColorInfo->parseXml(color_node))
				{
					delete_and_clear(mTexHairColorInfo);
					llwarns << "avatar file: mTexHairColor->parseXml() failed"
							<< llendl;
					return false;
				}
			}
			else if (global_color_name == "eye_color")
			{
				if (mTexEyeColorInfo)
				{
					llwarns << "avatar file: multiple instances of eye_color"
							<< llendl;
					return false;
				}
				mTexEyeColorInfo = new LLTexGlobalColorInfo;
				if (!mTexEyeColorInfo->parseXml(color_node))
				{
					delete_and_clear(mTexEyeColorInfo);
					llwarns << "avatar file: mTexEyeColor->parseXml() failed"
							<< llendl;
					return false;
				}
			}
		}
	}

	return true;
}

// Parses <layer_set> nodes from XML tree
bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlLayerNodes(LLXmlTreeNode* root)
{
	for (LLXmlTreeNode* layer_node = root->getChildByName("layer_set");
		 layer_node; layer_node = root->getNextNamedChild())
	{
		LLTexLayerSetInfo* layer_info = new LLTexLayerSetInfo();
		if (layer_info->parseXml(layer_node))
		{
			mLayerInfoList.push_back(layer_info);
		}
		else
		{
			llwarns << "avatar file: layer_set->parseXml() failed" << llendl;
			delete layer_info;
			return false;
		}
	}
	return true;
}

// Parses <driver_parameters> nodes from XML tree
bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlDriverNodes(LLXmlTreeNode* root)
{
	LLXmlTreeNode* driver = root->getChildByName("driver_parameters");
	if (driver)
	{
		for (LLXmlTreeNode* grand_child = driver->getChildByName("param");
			 grand_child;
			 grand_child = driver->getNextNamedChild())
		{
			if (grand_child->getChildByName("param_driver"))
			{
				LLDriverParamInfo* driver_info = new LLDriverParamInfo();
				if (driver_info->parseXml(grand_child))
				{
					mDriverInfoList.push_back(driver_info);
				}
				else
				{
					delete driver_info;
					llwarns << "avatar file: driver_param->parseXml() failed"
							<< llendl;
					return false;
				}
			}
		}
	}
	return true;
}

// Parses <driver_parameters> nodes from XML tree
bool LLAvatarAppearance::LLAvatarXmlInfo::parseXmlMorphNodes(LLXmlTreeNode* root)
{
	LLXmlTreeNode* masks = root->getChildByName("morph_masks");
	if (!masks)
	{
		return false;
	}

	for (LLXmlTreeNode* grand_child = masks->getChildByName("mask");
		 grand_child; grand_child = masks->getNextNamedChild())
	{
		LLAvatarMorphInfo* info = new LLAvatarMorphInfo();

		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("morph_name");
		if (!grand_child->getFastAttributeString(name_string, info->mName))
		{
			llwarns << "No name supplied for morph mask." << llendl;
			delete info;
			return false;
		}

		static LLStdStringHandle region_string =
			LLXmlTree::addAttributeString("body_region");
		if (!grand_child->getFastAttributeString(region_string, info->mRegion))
		{
			llwarns << "No region supplied for morph mask." << llendl;
			delete info;
			return false;
		}

		static LLStdStringHandle layer_string =
			LLXmlTree::addAttributeString("layer");
		if (!grand_child->getFastAttributeString(layer_string, info->mLayer))
		{
			llwarns << "No layer supplied for morph mask." << llendl;
			delete info;
			return false;
		}

		// optional parameter. don't throw a warning if not present.
		static LLStdStringHandle invert_string =
			LLXmlTree::addAttributeString("invert");
		grand_child->getFastAttributeBool(invert_string, info->mInvert);

		mMorphMaskInfoList.push_back(info);
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLMaskedMorph sub-class
//-----------------------------------------------------------------------------

//virtual
LLAvatarAppearance::LLMaskedMorph::LLMaskedMorph(LLVisualParam* morph_target,
												 bool invert,
												 const std::string& layer)
:	mMorphTarget(morph_target),
	mInvert(invert),
	mLayer(layer)
{
	LLPolyMorphTarget* target = morph_target->asPolyMorphTarget();
	if (target)
	{
		target->addPendingMorphMask();
	}
}

//-----------------------------------------------------------------------------
// LLAvatarAppearance class
//-----------------------------------------------------------------------------

LLAvatarAppearance::LLAvatarAppearance(LLWearableData* wearable_data)
:	LLCharacter(),
	mIsDummy(false),
	mIsBuilt(false),
	mTexSkinColor(NULL),
	mTexHairColor(NULL),
	mTexEyeColor(NULL),
	mNumBones(0),
	mPelvisToFoot(0.f),
	mHeadOffset(),
	mRoot(NULL),
	mWearableData(wearable_data)
{
	llassert_always(mWearableData);
	mBakedTextureDatas.resize(BAKED_NUM_INDICES);
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		mBakedTextureDatas[i].mLastTextureID = IMG_DEFAULT_AVATAR;
		mBakedTextureDatas[i].mTexLayerSet = NULL;
		mBakedTextureDatas[i].mIsLoaded = false;
		mBakedTextureDatas[i].mIsUsed = false;
		mBakedTextureDatas[i].mMaskTexName = 0;
		mBakedTextureDatas[i].mTextureIndex =
			LLAvatarAppearanceDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)i);
	}
}

//virtual
void LLAvatarAppearance::initInstance()
{
	// Initialize joint, mesh and shape members
	mRoot = createAvatarJoint();
	mRoot->setName("mRoot");

	for (LLAvatarAppearanceDictionary::MeshEntries::const_iterator
			iter = gAvatarAppDictp->getMeshEntries().begin(),
			end = gAvatarAppDictp->getMeshEntries().end();
		 iter != end; ++iter)
	{
		const EMeshIndex mesh_index = iter->first;
		const LLAvatarAppearanceDictionary::MeshEntry* mesh_dict = iter->second;
		LLAvatarJoint* joint = createAvatarJoint();
		joint->setName(mesh_dict->mName);
		joint->setMeshID(mesh_index);
		mMeshLOD.push_back(joint);

		for (U32 lod = 0, count = mesh_dict->mLOD; lod < count; ++lod)
		{
			LLAvatarJointMesh* mesh = createAvatarJointMesh();
			std::string mesh_name = "m" + mesh_dict->mName +
									boost::lexical_cast<std::string>(lod);
			// We pre-pended an m - need to capitalize first character for
			// camelCase
			mesh_name[1] = toupper(mesh_name[1]);
			mesh->setName(mesh_name);
			mesh->setMeshID(mesh_index);
			mesh->setPickName(mesh_dict->mPickName);
			mesh->setIsTransparent(false);
			switch ((S32)mesh_index)
			{
				case MESH_ID_HAIR:
					mesh->setIsTransparent(true);
					break;

				case MESH_ID_SKIRT:
					mesh->setIsTransparent(true);
					break;

				case MESH_ID_EYEBALL_LEFT:
				case MESH_ID_EYEBALL_RIGHT:
					mesh->setSpecular(LLColor4(1.f, 1.f, 1.f, 1.f), 1.f);
			}

			joint->mMeshParts.push_back(mesh);
		}
	}

	// Associate baked textures with meshes
	for (LLAvatarAppearanceDictionary::MeshEntries::const_iterator
				it1 = gAvatarAppDictp->getMeshEntries().begin(),
				end1 = gAvatarAppDictp->getMeshEntries().end();
		 it1 != end1; ++it1)
	{
		const EMeshIndex mesh_index = it1->first;
		const LLAvatarAppearanceDictionary::MeshEntry* mesh_dict = it1->second;
		const EBakedTextureIndex btex_idx = mesh_dict->mBakedID;
		// Skip it if there is no associated baked texture.
		if (btex_idx == BAKED_NUM_INDICES) continue;

		for (avatar_joint_mesh_list_t::iterator
				it2 = mMeshLOD[mesh_index]->mMeshParts.begin(),
				end2 = mMeshLOD[mesh_index]->mMeshParts.end();
			 it2 != end2; ++it2)
		{
			LLAvatarJointMesh* mesh = *it2;
			mBakedTextureDatas[(S32)btex_idx].mJointMeshes.push_back(mesh);
		}
	}

	buildCharacter();
}

//virtual
LLAvatarAppearance::~LLAvatarAppearance()
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if (mBakedTextureDatas[i].mTexLayerSet)
		{
			delete mBakedTextureDatas[i].mTexLayerSet;
			mBakedTextureDatas[i].mTexLayerSet = NULL;
		}
		mBakedTextureDatas[i].mJointMeshes.clear();

		for (morph_list_t::iterator
				iter = mBakedTextureDatas[i].mMaskedMorphs.begin(),
				end = mBakedTextureDatas[i].mMaskedMorphs.end();
			 iter != end; ++iter)
		{
			LLMaskedMorph* masked_morph = *iter;
			delete masked_morph;
		}
	}

	if (mRoot)
	{
		mRoot->removeAllChildren();
		delete mRoot;
		mRoot = NULL;
	}
	mJointMap.clear();

	clearSkeleton();

	clearCollisionVolumes();

	delete_and_clear(mTexSkinColor);
	delete_and_clear(mTexHairColor);
	delete_and_clear(mTexEyeColor);

	std::for_each(mPolyMeshes.begin(), mPolyMeshes.end(),
				  DeletePairedPointer());
	mPolyMeshes.clear();

	for (avatar_joint_list_t::iterator iter = mMeshLOD.begin(),
									   end = mMeshLOD.end();
		 iter != end; ++iter)
	{
		LLAvatarJoint* joint = *iter;
		std::for_each(joint->mMeshParts.begin(), joint->mMeshParts.end(),
					   DeletePointer());
		joint->mMeshParts.clear();
	}
	std::for_each(mMeshLOD.begin(), mMeshLOD.end(), DeletePointer());
	mMeshLOD.clear();

	delete_and_clear(mRoot);
}

//static
void LLAvatarAppearance::initClass(const std::string& lad_file,
								   const std::string& skel_file)
{
	std::string avatar_file = lad_file.empty() ? AVATAR_DEFAULT_CHAR
											   : lad_file;
	avatar_file = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,
												 avatar_file);
	LLXmlTree lad_xml_tree;
	if (!lad_xml_tree.parseFile(avatar_file, false))
	{
		llerrs << "Problem reading avatar configuration file:" << avatar_file
			   << llendl;
	}

	// Now sanity check xml file
	LLXmlTreeNode* root = lad_xml_tree.getRoot();
	if (!root)
	{
		llerrs << "No root node found in avatar configuration file: "
			   << avatar_file << llendl;
	}

	// <linden_avatar version="M.n"> (root)
	if (!root->hasName("linden_avatar"))
	{
		llerrs << "Invalid avatar file header: " << avatar_file << llendl;
	}

	std::string version;
	static LLStdStringHandle version_string =
		LLXmlTree::addAttributeString("version");
	if (!root->getFastAttributeString(version_string, version) ||
		(version != "1.0" && version != "2.0"))
	{
		llerrs << "Invalid avatar file version: " << version << " in file: "
			   << avatar_file << llendl;
	}

	S32 wearable_def_version = 1;
	static LLStdStringHandle wearable_version_string =
		LLXmlTree::addAttributeString("wearable_definition_version");
	root->getFastAttributeS32(wearable_version_string,
							  wearable_def_version);
	LLWearable::setCurrentDefinitionVersion(wearable_def_version);

	std::string mesh_file_name;

	LLXmlTreeNode* skeleton_node = root->getChildByName("skeleton");
	if (!skeleton_node)
	{
		llerrs << "No skeleton in avatar configuration file: " << avatar_file
			   << llendl;
	}

	std::string skeleton_file_name = skel_file;
	if (skel_file.empty())
	{
		static LLStdStringHandle filename_str =
			LLXmlTree::addAttributeString("file_name");
		if (!skeleton_node->getFastAttributeString(filename_str,
												   skeleton_file_name))
		{
			llerrs << "No file name in skeleton node in avatar config file: "
				   << avatar_file << llendl;
		}
	}

	std::string skeleton_path;
	skeleton_path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,
												   skeleton_file_name);
	LLXmlTree skel_xml_tree;
	if (!parseSkeletonFile(skeleton_path, skel_xml_tree))
	{
		llerrs << "Error parsing skeleton file: " << skeleton_path << llendl;
	}

	// Process XML data

	// avatar_skeleton.xml
	if (sAvatarSkeletonInfo)
	{
		// This can happen if a login attempt failed
		delete sAvatarSkeletonInfo;
	}
	sAvatarSkeletonInfo = new LLAvatarSkeletonInfo;
	if (!sAvatarSkeletonInfo->parseXml(skel_xml_tree.getRoot()))
	{
		llerrs << "Error parsing skeleton XML file: " << skeleton_path
			   << llendl;
	}

	// Parse avatar_lad.xml

	// Release old sAvatarXmlInfo if any (this can happen if a login attempt
	// failed)
	delete_and_clear(sAvatarXmlInfo);

	sAvatarXmlInfo = new LLAvatarXmlInfo;
	if (!sAvatarXmlInfo->parseXmlSkeletonNode(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
	if (!sAvatarXmlInfo->parseXmlMeshNodes(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
	if (!sAvatarXmlInfo->parseXmlColorNodes(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
	if (!sAvatarXmlInfo->parseXmlLayerNodes(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
	if (!sAvatarXmlInfo->parseXmlDriverNodes(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
	if (!sAvatarXmlInfo->parseXmlMorphNodes(root))
	{
		llerrs << "Error parsing skeleton node in avatar XML file: "
			   << skeleton_path << llendl;
	}
}

void LLAvatarAppearance::cleanupClass()
{
	delete_and_clear(sAvatarXmlInfo);
	delete_and_clear(sAvatarSkeletonInfo);
}

// The viewer can only suggest a good size for the agent, the simulator will
// keep it inside a reasonable range.
void LLAvatarAppearance::computeBodySize(bool force)
{
	LLVector3 pelvis_scale = mPelvisp->getScale();

	// Some of the joints have not been cached
	LLVector3 skull = mSkullp->getPosition();

	LLVector3 neck = mNeckp->getPosition();
	LLVector3 neck_scale = mNeckp->getScale();

	LLVector3 chest = mChestp->getPosition();
	LLVector3 chest_scale = mChestp->getScale();

	// The rest of the joints have been cached
	LLVector3 head = mHeadp->getPosition();
	LLVector3 head_scale = mHeadp->getScale();

	LLVector3 torso = mTorsop->getPosition();
	LLVector3 torso_scale = mTorsop->getScale();

	LLVector3 hip = mHipLeftp->getPosition();
	LLVector3 hip_scale = mHipLeftp->getScale();

	LLVector3 knee = mKneeLeftp->getPosition();
	LLVector3 knee_scale = mKneeLeftp->getScale();

	LLVector3 ankle = mAnkleLeftp->getPosition();
	LLVector3 ankle_scale = mAnkleLeftp->getScale();

	LLVector3 foot = mFootLeftp->getPosition();

	mPelvisToFoot = hip.mV[VZ] * pelvis_scale.mV[VZ] -
				 	knee.mV[VZ] * hip_scale.mV[VZ] -
				 	ankle.mV[VZ] * knee_scale.mV[VZ] -
				 	foot.mV[VZ] * ankle_scale.mV[VZ];

	LLVector3 new_body_size;
	new_body_size.mV[VZ] = mPelvisToFoot +
						  // The sqrt(2) correction below is an approximate
						  // correction to get to the top of the head
						  F_SQRT2 * (skull.mV[VZ] * head_scale.mV[VZ]) +
						  head.mV[VZ] * neck_scale.mV[VZ] +
						  neck.mV[VZ] * chest_scale.mV[VZ] +
						  chest.mV[VZ] * torso_scale.mV[VZ] +
						  torso.mV[VZ] * pelvis_scale.mV[VZ];

	// *TODO: measure the real depth and width
	new_body_size.mV[VX] = DEFAULT_AGENT_DEPTH;
	new_body_size.mV[VY] = DEFAULT_AGENT_WIDTH;

	mAvatarOffset.mV[VX] = mAvatarOffset.mV[VY] = 0.0f;

	F32 old_offset = mAvatarOffset.mV[VZ];
	mAvatarOffset.mV[VZ] = getVisualParamWeight(AVATAR_HOVER);
	bool offset_changed = old_offset != mAvatarOffset.mV[VZ];

	if (new_body_size != mBodySize || offset_changed)
	{
		mBodySize = new_body_size;
		bodySizeChanged();
	}
}

bool LLAvatarAppearance::parseSkeletonFile(const std::string& filename,
										   LLXmlTree& skel_xml_tree)
{
	// Parse the file
	if (!skel_xml_tree.parseFile(filename, false))
	{
		llerrs << "Cannot parse skeleton file: " << filename << llendl;
	}

	// Now sanity-check the XML
	LLXmlTreeNode* root = skel_xml_tree.getRoot();
	if (!root)
	{
		llerrs << "No root node found in avatar skeleton file: " << filename
			   << llendl;
	}

	if (!root->hasName("linden_skeleton"))
	{
		llerrs << "Invalid avatar skeleton file header: " << filename
			   << llendl;
	}

	std::string version;
	static LLStdStringHandle version_string =
			LLXmlTree::addAttributeString("version");
	if (!root->getFastAttributeString(version_string, version) ||
		(version != "1.0" && version != "2.0"))
	{
		llerrs << "Invalid avatar skeleton file version: " << version
			   << " in file: " << filename << llendl;
	}

	return true;
}

bool LLAvatarAppearance::setupBone(const LLAvatarBoneInfo* info,
								   LLJoint* parent,
								   S32& volume_num,
								   S32& joint_num)
{
	if (!info)
	{
		llwarns << "NULL avatar bone info pointer passed." << llendl;
		return false;
	}

	LLJoint* joint = NULL;

	LL_DEBUGS("Avatar") << "Bone info. Name: " << info->mName
						<< " - Joint: " << (info->mIsJoint ? "yes" : "no")
						<< " - Volume number: " << volume_num
						<< " - Joint number: " << joint_num << LL_ENDL;

	if (info->mIsJoint)
	{
		joint = getCharacterJoint(joint_num);
		if (!joint)
		{
			llwarns << "Too many bones" << llendl;
			return false;
		}
		joint->setName(info->mName);
	}
	else	// Collision volume
	{
		if (volume_num >= (S32)mCollisionVolumes.size())
		{
			llwarns << "Too many collision volumes" << llendl;
			return false;
		}
		joint = mCollisionVolumes[volume_num];
		joint->setName(info->mName);
	}

	// Add to parent
	if (parent && joint->getParent() != parent)
	{
		parent->addChild(joint);
	}

	joint->setPosition(info->mPos);
	joint->setDefaultPosition(info->mPos);
	joint->setRotation(mayaQ(info->mRot.mV[VX], info->mRot.mV[VY],
							 info->mRot.mV[VZ], LLQuaternion::XYZ));
	joint->setScale(info->mScale);
	joint->setDefaultScale(info->mScale);
	joint->setSupport(info->mSupport);
	joint->setEnd(info->mEnd);

	if (info->mIsJoint)
	{
		joint->setSkinOffset(info->mPivot);
		joint->setJointNum(joint_num++);
		joint->setIsBone(true);
	}
	else // collision volume
	{
		joint->setJointNum(mNumBones + volume_num++);
	}

	// Setup children
	for (LLAvatarBoneInfo::child_list_t::const_iterator
			iter = info->mChildList.begin(), end = info->mChildList.end();
		 iter != end; ++iter)
	{
		LLAvatarBoneInfo* child_info = *iter;
		if (!setupBone(child_info, joint, volume_num, joint_num))
		{
			return false;
		}
	}

	return true;
}

bool LLAvatarAppearance::allocateCharacterJoints(U32 num)
{
	if (num != (U32)mSkeleton.size())
	{
		clearSkeleton();
		mSkeleton = avatar_joint_list_t(num, NULL);
		mNumBones = num;
	}
	return true;
}

bool LLAvatarAppearance::buildSkeleton(const LLAvatarSkeletonInfo* info)
{
	if (!info)
	{
		llwarns << "NULL avatar skeleton info pointer passed." << llendl;
		return false;
	}

	LL_DEBUGS("Avatar") << "Sketeton info. Bones: " << info->mNumBones
						<< " - Collision volumes: "
						<< info->mNumCollisionVolumes << LL_ENDL;

	// Allocate joints
	if (!allocateCharacterJoints(info->mNumBones))
	{
		llerrs << "Cannot allocate " << info->mNumBones << " joints" << llendl;
	}

	// Allocate volumes
	if (info->mNumCollisionVolumes)
	{
		if (!allocateCollisionVolumes(info->mNumCollisionVolumes))
		{
			llerrs << "Cannot allocate " << info->mNumCollisionVolumes
				   << " collision volumes" << llendl;
		}
	}

	S32 current_joint_num = 0;
	S32 current_volume_num = 0;
	for (LLAvatarSkeletonInfo::bone_info_list_t::const_iterator
		iter = info->mBoneInfoList.begin(), end = info->mBoneInfoList.end();
		iter != end; ++iter)
	{
		LLAvatarBoneInfo* bone_info = *iter;
		if (!setupBone(bone_info, NULL, current_volume_num, current_joint_num))
		{
			llerrs << "Error parsing bone in skeleton file" << llendl;
		}
	}

	return true;
}

void LLAvatarAppearance::clearSkeleton()
{
	std::for_each(mSkeleton.begin(), mSkeleton.end(), DeletePointer());
	mSkeleton.clear();
}

void LLAvatarAppearance::addPelvisFixup(F32 fixup, const LLUUID& mesh_id)
{
	if (mesh_id.notNull())
	{
		LLVector3 pos(0.f, 0.f, fixup);
		mPelvisFixups.add(mesh_id, pos);
	}
}

void LLAvatarAppearance::removePelvisFixup(const LLUUID& mesh_id)
{
	mPelvisFixups.remove(mesh_id);
}

bool LLAvatarAppearance::hasPelvisFixup(F32& fixup, LLUUID& mesh_id) const
{
	LLVector3 pos;
	if (mPelvisFixups.findActiveOverride(mesh_id, pos))
	{
		fixup = pos[2];
		return true;
	}
	return false;
}

bool LLAvatarAppearance::hasPelvisFixup(F32& fixup) const
{
	LLUUID mesh_id;
	return hasPelvisFixup(fixup, mesh_id);
}

// Deferred initialization and rebuild of the avatar.
void LLAvatarAppearance::buildCharacter()
{
	// Remove all references to our existing skeleton so we can rebuild it
	deactivateAllMotions();

	// Remove all of mRoot's children
	mRoot->removeAllChildren();
	mJointMap.clear();
	mIsBuilt = false;

	// Clear mesh data
	for (avatar_joint_list_t::iterator it1 = mMeshLOD.begin(),
									   end1 = mMeshLOD.end();
		 it1 != end1; ++it1)
	{
		LLAvatarJoint* joint = *it1;
		for (avatar_joint_mesh_list_t::iterator
				it2 = joint->mMeshParts.begin(),
				end2 = joint->mMeshParts.end();
			 it2 != end2; ++it2)
		{
			LLAvatarJointMesh* mesh = *it2;
			mesh->setMesh(NULL);
		}
	}

	// (Re)load our skeleton and meshes
	LLTimer timer;
	if (!loadAvatar())
	{
		if (isSelf())
		{
			llerrs << "Unable to load user's avatar" << llendl;
		}

		llwarns << "Unable to load other's avatar" << llendl;
		return;
	}

	LL_DEBUGS("Avatar") << "Avatar load took " << timer.getElapsedTimeF32()
						<< " seconds." << LL_ENDL;

	// Initialize "well known" joint pointers
	mPelvisp		= mRoot->findJoint(LL_JOINT_KEY_PELVIS);
	mTorsop			= mRoot->findJoint(LL_JOINT_KEY_TORSO);
	mChestp			= mRoot->findJoint(LL_JOINT_KEY_CHEST);
	mNeckp			= mRoot->findJoint(LL_JOINT_KEY_NECK);
	mHeadp			= mRoot->findJoint(LL_JOINT_KEY_HEAD);
	mSkullp			= mRoot->findJoint(LL_JOINT_KEY_SKULL);
	mHipLeftp		= mRoot->findJoint(LL_JOINT_KEY_HIPLEFT);
	mHipRightp		= mRoot->findJoint(LL_JOINT_KEY_HIPRIGHT);
	mKneeLeftp		= mRoot->findJoint(LL_JOINT_KEY_KNEELEFT);
	mKneeRightp		= mRoot->findJoint(LL_JOINT_KEY_KNEERIGHT);
	mAnkleLeftp		= mRoot->findJoint(LL_JOINT_KEY_ANKLELEFT);
	mAnkleRightp	= mRoot->findJoint(LL_JOINT_KEY_ANKLERIGHT);
	mFootLeftp		= mRoot->findJoint(LL_JOINT_KEY_FOOTLEFT);
	mFootRightp		= mRoot->findJoint(LL_JOINT_KEY_FOOTRIGHT);
	mWristLeftp		= mRoot->findJoint(LL_JOINT_KEY_WRISTLEFT);
	mWristRightp	= mRoot->findJoint(LL_JOINT_KEY_WRISTRIGHT);
	mEyeLeftp		= mRoot->findJoint(LL_JOINT_KEY_EYELEFT);
	mEyeRightp		= mRoot->findJoint(LL_JOINT_KEY_EYERIGHT);

	// Make sure "well known" pointers exist
	if (!(mPelvisp && mTorsop && mChestp && mNeckp && mHeadp && mSkullp &&
		  mHipLeftp && mHipRightp && mKneeLeftp && mKneeRightp &&
		  mAnkleLeftp && mAnkleRightp && mFootLeftp && mFootRightp &&
		  mWristLeftp && mWristRightp && mEyeLeftp && mEyeRightp))
	{
		llerrs << "Failed to create avatar." << llendl;
	}

	// Initialize the pelvis
	mPelvisp->setPosition(LLVector3::zero);

	mIsBuilt = true;
}

bool LLAvatarAppearance::loadAvatar()
{
	// avatar_skeleton.xml
	if (!buildSkeleton(sAvatarSkeletonInfo))
	{
		llwarns << "Avatar file: buildSkeleton() failed" << llendl;
		return false;
	}

	if (LLJoint::sAvatarJointAliasMap.empty())
	{
		LLJoint::sAvatarJointAliasMap = getJointAliases();
		LL_DEBUGS("Avatar") << "Avatar skeleton joints aliases:";
		for (joint_alias_map_t::const_iterator
				it = LLJoint::sAvatarJointAliasMap.begin(),
				end = LLJoint::sAvatarJointAliasMap.end();
			 it != end; ++it)
		{
			LL_CONT << "\n    " << it->first << " -> " << it->second;
		}
		LL_CONT << LL_ENDL;
	}

	// avatar_lad.xml : <skeleton>
	if (!loadSkeletonNode())
	{
		llerrs << "Avatar file: loadNodeSkeleton() failed" << llendl;
	}

	// avatar_lad.xml : <mesh>
	if (!loadMeshNodes())
	{
		llerrs << "Avatar file: loadNodeMesh() failed" << llendl;
	}

	// avatar_lad.xml : <global_color>
	if (sAvatarXmlInfo->mTexSkinColorInfo)
	{
		mTexSkinColor = new LLTexGlobalColor(this);
		if (!mTexSkinColor->setInfo(sAvatarXmlInfo->mTexSkinColorInfo))
		{
			llerrs << "Avatar file: mTexSkinColor->setInfo() failed" << llendl;
		}
	}
	else
	{
		llerrs << "<global_color> name=\"skin_color\" not found" << llendl;
	}

	if (sAvatarXmlInfo->mTexHairColorInfo)
	{
		mTexHairColor = new LLTexGlobalColor(this);
		if (!mTexHairColor->setInfo(sAvatarXmlInfo->mTexHairColorInfo))
		{
			llerrs << "Avatar file: mTexHairColor->setInfo() failed" << llendl;
		}
	}
	else
	{
		llerrs << "<global_color> name=\"hair_color\" not found" << llendl;
	}

	if (sAvatarXmlInfo->mTexEyeColorInfo)
	{
		mTexEyeColor = new LLTexGlobalColor(this);
		if (!mTexEyeColor->setInfo(sAvatarXmlInfo->mTexEyeColorInfo))
		{
			llerrs << "Avatar file: mTexEyeColor->setInfo() failed" << llendl;
		}
	}
	else
	{
		llerrs << "<global_color> name=\"eye_color\" not found" << llendl;
	}

	// avatar_lad.xml : <layer_set>
	if (sAvatarXmlInfo->mLayerInfoList.empty())
	{
		llerrs << "Avatar file: missing <layer_set> node" << llendl;
	}

	if (sAvatarXmlInfo->mMorphMaskInfoList.empty())
	{
		llerrs << "Avatar file: missing <morph_masks> node" << llendl;
	}

	// avatar_lad.xml : <morph_masks>
	for (LLAvatarXmlInfo::morph_info_list_t::iterator
			iter = sAvatarXmlInfo->mMorphMaskInfoList.begin(),
			end = sAvatarXmlInfo->mMorphMaskInfoList.end();
		 iter != end;  ++iter)
	{
		LLAvatarXmlInfo::LLAvatarMorphInfo* info = *iter;

		EBakedTextureIndex baked =
			LLAvatarAppearanceDictionary::findBakedByRegionName(info->mRegion);
		if (baked != BAKED_NUM_INDICES)
		{
			LLVisualParam* morph_param = getVisualParam(info->mName.c_str());
			if (morph_param)
			{
				addMaskedMorph(baked, morph_param, info->mInvert,
							   info->mLayer);
			}
		}
	}

	loadLayersets();

	// avatar_lad.xml : <driver_parameters>
	for (LLAvatarXmlInfo::driver_info_list_t::iterator
			iter = sAvatarXmlInfo->mDriverInfoList.begin(),
			end = sAvatarXmlInfo->mDriverInfoList.end();
		 iter != end; ++iter)
	{
		LLDriverParamInfo* info = *iter;
		LLDriverParam* driver_param = new LLDriverParam(this);
		if (driver_param->setInfo(info))
		{
			addVisualParam(driver_param);
			driver_param->setParamLocation(isSelf() ? LOC_AV_SELF
													: LOC_AV_OTHER);
			LLVisualParam*(LLAvatarAppearance::*avatar_function)(S32)const =
				&LLAvatarAppearance::getVisualParam;
			if (!driver_param->linkDrivenParams(boost::bind(avatar_function,
															(LLAvatarAppearance*)this,
															_1),
												false))
			{
				llwarns << "Could not link driven params for avatar "
						<< getID().asString() << " param id: "
						<< driver_param->getID() << llendl;
				continue;
			}
		}
		else
		{
			delete driver_param;
			llwarns << "driver_param->setInfo() failed" << llendl;
			return false;
		}
	}

	return true;
}

// Loads <skeleton> node from XML tree
bool LLAvatarAppearance::loadSkeletonNode()
{
	mRoot->addChild(mSkeleton[0]);

	// Make meshes children before calling parent version of the function
	for (avatar_joint_list_t::iterator iter = mMeshLOD.begin(),
									   end = mMeshLOD.end();
		 iter != end; ++iter)
	{
		LLAvatarJoint* joint = *iter;
		joint->mUpdateXform = false;
		joint->setMeshesToChildren();
	}

	mRoot->addChild(mMeshLOD[MESH_ID_HEAD]);
	mRoot->addChild(mMeshLOD[MESH_ID_EYELASH]);
	mRoot->addChild(mMeshLOD[MESH_ID_UPPER_BODY]);
	mRoot->addChild(mMeshLOD[MESH_ID_LOWER_BODY]);
	mRoot->addChild(mMeshLOD[MESH_ID_SKIRT]);

	LLJoint* skull = mRoot->findJoint(LL_JOINT_KEY_SKULL);
	if (skull)
	{
		skull->addChild(mMeshLOD[MESH_ID_HAIR]);
	}

	LLJoint* l_eye = mRoot->findJoint(LL_JOINT_KEY_EYELEFT);
	if (l_eye)
	{
		l_eye->addChild(mMeshLOD[MESH_ID_EYEBALL_LEFT]);
	}

	LLJoint* r_eye = mRoot->findJoint(LL_JOINT_KEY_EYERIGHT);
	if (r_eye)
	{
		r_eye->addChild(mMeshLOD[MESH_ID_EYEBALL_RIGHT]);
	}

	// SKELETAL DISTORTIONS
	for (LLAvatarXmlInfo::skeletal_distortion_info_list_t::iterator
			iter = sAvatarXmlInfo->mSkeletalDistortionInfoList.begin(),
			end = sAvatarXmlInfo->mSkeletalDistortionInfoList.end();
		 iter != end; ++iter)
	{
		LLPolySkeletalDistortionInfo* info =
			(LLPolySkeletalDistortionInfo*)*iter;
		LLPolySkeletalDistortion* param = new LLPolySkeletalDistortion(this);
		if (!param->setInfo(info))
		{
			delete param;
			return false;
		}

		addVisualParam(param);
		param->setParamLocation(isSelf() ? LOC_AV_SELF : LOC_AV_OTHER);
	}

	return true;
}

// Loads <mesh> nodes from XML tree
bool LLAvatarAppearance::loadMeshNodes()
{
	for (LLAvatarXmlInfo::mesh_info_list_t::const_iterator
			it1 = sAvatarXmlInfo->mMeshInfoList.begin(),
			end1 = sAvatarXmlInfo->mMeshInfoList.end();
		 it1 != end1; ++it1)
	{
		const LLAvatarXmlInfo::LLAvatarMeshInfo* info = *it1;
		const std::string &type = info->mType;
		S32 lod = info->mLOD;

		U8 mesh_id = 0;
		bool found_mesh_id = false;
		for (LLAvatarAppearanceDictionary::MeshEntries::const_iterator
				it2 = gAvatarAppDictp->getMeshEntries().begin(),
				end2 = gAvatarAppDictp->getMeshEntries().end();
			 it2 != end2; ++it2)
		{
			const EMeshIndex mesh_index = it2->first;
			const LLAvatarAppearanceDictionary::MeshEntry* mesh_dict =
				it2->second;
			if (type.compare(mesh_dict->mName) == 0)
			{
				mesh_id = mesh_index;
				found_mesh_id = true;
				break;
			}
		}
		if (!found_mesh_id)
		{
			llwarns << "Ignoring unrecognized mesh type: " << type << llendl;
			return false;
		}
		if (lod >= (S32)mMeshLOD[mesh_id]->mMeshParts.size())
		{
			llwarns << "Avatar file: <mesh> has invalid lod setting " << lod
					<< llendl;
			return false;
		}
		LLAvatarJointMesh* mesh = mMeshLOD[mesh_id]->mMeshParts[lod];

		// If this is not set to white (1.0), avatars will *ALWAYS* be darker
		// than their surroundings. Do not touch !!!
		mesh->setColor(LLColor4::white);

		LLPolyMesh* poly_mesh;
		if (!info->mReferenceMeshName.empty())
		{
			polymesh_map_t::const_iterator it3 =
				mPolyMeshes.find(info->mReferenceMeshName);
			if (it3 == mPolyMeshes.end())
			{
				// This should never happen
				llwarns << "Could not find avatar mesh: "
						<< info->mReferenceMeshName << llendl;
				return false;
			}

			poly_mesh = LLPolyMesh::getMesh(info->mMeshFileName, it3->second);
			if (poly_mesh)
			{
				poly_mesh->setAvatar(this);
			}
		}
		else
		{
			poly_mesh = LLPolyMesh::getMesh(info->mMeshFileName);
			if (poly_mesh)
			{
				poly_mesh->setAvatar(this);
			}
		}

		if (!poly_mesh)
		{
			llwarns << "Failed to load mesh of type " << type << llendl;
			return false;
		}

		// Multimap insert
		mPolyMeshes.emplace(info->mMeshFileName, poly_mesh);

		mesh->setMesh(poly_mesh);
		mesh->setLOD(info->mMinPixelArea);

		for (LLAvatarXmlInfo::LLAvatarMeshInfo::morph_info_list_t::const_iterator
				it4 = info->mPolyMorphTargetInfoList.begin(),
				end4 = info->mPolyMorphTargetInfoList.end();
			 it4 != end4; ++it4)
		{
			const LLAvatarXmlInfo::LLAvatarMeshInfo::morph_info_pair_t* info_pair =
				&(*it4);

			LLPolyMorphTarget* param = new LLPolyMorphTarget(mesh->getMesh());
			if (!param->setInfo((LLPolyMorphTargetInfo*)info_pair->first))
			{
				delete param;
				return false;
			}

			if (info_pair->second)
			{
				addSharedVisualParam(param);
			}
			else
			{
				addVisualParam(param);
			}
			param->setParamLocation(isSelf() ? LOC_AV_SELF : LOC_AV_OTHER);
		}
	}

	return true;
}

bool LLAvatarAppearance::loadLayersets()
{
	bool success = true;
	for (LLAvatarXmlInfo::layer_info_list_t::const_iterator
			it1 = sAvatarXmlInfo->mLayerInfoList.begin(),
			end1 = sAvatarXmlInfo->mLayerInfoList.end();
		 it1 != end1; ++it1)
	{
		LLTexLayerSetInfo* layerset_info = *it1;
		if (isSelf())
		{
			// Construct a layerset for each one specified in avatar_lad.xml
			// and initialize it as such.
			LLTexLayerSet* layer_set = createTexLayerSet();
			if (!layer_set->setInfo(layerset_info))
			{
				delete layer_set;
				llwarns << "avatar file: layer_set->setInfo() failed"
						<< llendl;
				return false;
			}

			// Scan baked textures and associate the layerset with the
			// appropriate one
			EBakedTextureIndex baked_index = BAKED_NUM_INDICES;
			for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator
					it2 = gAvatarAppDictp->getBakedTextures().begin(),
					end2 = gAvatarAppDictp->getBakedTextures().end();
				 it2 != end2; ++it2)
			{
				const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
					it2->second;
				if (layer_set->isBodyRegion(baked_dict->mName))
				{
					baked_index = it2->first;
					// Ensure both structures are aware of each other
					mBakedTextureDatas[baked_index].mTexLayerSet = layer_set;
					layer_set->setBakedTexIndex(baked_index);
					break;
				}
			}
			// If no baked texture was found, warn and cleanup
			if (baked_index == BAKED_NUM_INDICES)
			{
				llwarns << "<layer_set> has invalid body_region attribute"
						<< llendl;
				delete layer_set;
				return false;
			}

			// Scan morph masks and let any affected layers know they have an
			// associated morph
			for (LLAvatarAppearance::morph_list_t::const_iterator
					it3 = mBakedTextureDatas[baked_index].mMaskedMorphs.begin(),
					end3 = mBakedTextureDatas[baked_index].mMaskedMorphs.end();
				it3 != end3; ++it3)
			{
				LLMaskedMorph* morph = *it3;
				LLTexLayerInterface* layer =
					layer_set->findLayerByName(morph->mLayer);
				if (layer)
				{
					layer->setHasMorph(true);
				}
				else
				{
					llwarns << "Could not find layer named " << morph->mLayer
							<< " to set morph flag" << llendl;
					success = false;
				}
			}
		}
		else // !isSelf()
		{
			// Construct a layerset for each one specified in avatar_lad.xml
			// and initialize it as such.
			LLTexLayerSetInfo* layerset_info = *it1;
			layerset_info->createVisualParams(this);
		}
	}
	return success;
}

LLJoint* LLAvatarAppearance::getCharacterJoint(U32 num)
{
	if ((S32)num < 0 || num >= (U32)mSkeleton.size())
	{
		return NULL;
	}
	if (!mSkeleton[num])
	{
		mSkeleton[num] = createAvatarJoint();
	}
	return mSkeleton[num];
}

LLVector3 LLAvatarAppearance::getVolumePos(S32 joint_index,
										   LLVector3& volume_offset)
{
	if (joint_index < 0 || joint_index > (S32)mCollisionVolumes.size())
	{
		return LLVector3::zero;
	}

	return mCollisionVolumes[joint_index]->getVolumePos(volume_offset);
}

LLJoint* LLAvatarAppearance::findCollisionVolume(S32 volume_id)
{
	if (volume_id < 0 || volume_id > (S32)mCollisionVolumes.size())
	{
		return NULL;
	}

	return mCollisionVolumes[volume_id];
}

S32 LLAvatarAppearance::getCollisionVolumeID(std::string& name)
{
	for (S32 i = 0, count = mCollisionVolumes.size(); i < count; ++i)
	{
		if (mCollisionVolumes[i]->getName() == name)
		{
			return i;
		}
	}

	return -1;
}

LLPolyMesh*	LLAvatarAppearance::getHeadMesh()
{
	return mMeshLOD[MESH_ID_HEAD]->mMeshParts[0]->getMesh();
}

LLPolyMesh*	LLAvatarAppearance::getUpperBodyMesh()
{
	return mMeshLOD[MESH_ID_UPPER_BODY]->mMeshParts[0]->getMesh();
}

LLPolyMesh* LLAvatarAppearance::getMesh(S32 which)
{
	return mMeshLOD[which]->mMeshParts[0]->getMesh();
}

// Adds a morph mask to the appropriate baked texture structure
void LLAvatarAppearance::addMaskedMorph(EBakedTextureIndex index,
										LLVisualParam* morph_target,
										bool invert, const std::string& layer)
{
	if (index < BAKED_NUM_INDICES)
	{
		LLMaskedMorph* morph = new LLMaskedMorph(morph_target, invert, layer);
		mBakedTextureDatas[index].mMaskedMorphs.push_front(morph);
	}
}

//static
bool LLAvatarAppearance::teToColorParams(ETextureIndex te, U32* param_name)
{
	switch (te)
	{
		case TEX_UPPER_SHIRT:
			param_name[0] = 803; // shirt_red
			param_name[1] = 804; // shirt_green
			param_name[2] = 805; // shirt_blue
			break;

		case TEX_LOWER_PANTS:
			param_name[0] = 806; // pants_red
			param_name[1] = 807; // pants_green
			param_name[2] = 808; // pants_blue
			break;

		case TEX_LOWER_SHOES:
			param_name[0] = 812; // shoes_red
			param_name[1] = 813; // shoes_green
			param_name[2] = 817; // shoes_blue
			break;

		case TEX_LOWER_SOCKS:
			param_name[0] = 818; // socks_red
			param_name[1] = 819; // socks_green
			param_name[2] = 820; // socks_blue
			break;

		case TEX_UPPER_JACKET:
		case TEX_LOWER_JACKET:
			param_name[0] = 834; // jacket_red
			param_name[1] = 835; // jacket_green
			param_name[2] = 836; // jacket_blue
			break;

		case TEX_UPPER_GLOVES:
			param_name[0] = 827; // gloves_red
			param_name[1] = 829; // gloves_green
			param_name[2] = 830; // gloves_blue
			break;

		case TEX_UPPER_UNDERSHIRT:
			param_name[0] = 821; // undershirt_red
			param_name[1] = 822; // undershirt_green
			param_name[2] = 823; // undershirt_blue
			break;

		case TEX_LOWER_UNDERPANTS:
			param_name[0] = 824; // underpants_red
			param_name[1] = 825; // underpants_green
			param_name[2] = 826; // underpants_blue
			break;

		case TEX_SKIRT:
			param_name[0] = 921; // skirt_red
			param_name[1] = 922; // skirt_green
			param_name[2] = 923; // skirt_blue
			break;

		case TEX_HEAD_TATTOO:
		case TEX_LOWER_TATTOO:
		case TEX_UPPER_TATTOO:
			param_name[0] = 1071; // tattoo_red
			param_name[1] = 1072; // tattoo_green
			param_name[2] = 1073; // tattoo_blue
			break;

		case TEX_HEAD_UNIVERSAL_TATTOO:
		case TEX_UPPER_UNIVERSAL_TATTOO:
		case TEX_LOWER_UNIVERSAL_TATTOO:
		case TEX_HAIR_TATTOO:
		case TEX_EYES_TATTOO:
		case TEX_LEFT_ARM_TATTOO:
		case TEX_LEFT_LEG_TATTOO:
		case TEX_SKIRT_TATTOO:
		case TEX_AUX1_TATTOO:
		case TEX_AUX2_TATTOO:
		case TEX_AUX3_TATTOO:
			param_name[0] = 1238; // tattoo_universal_red
			param_name[1] = 1239; // tattoo_universal_green
			param_name[2] = 1240; // tattoo_universal_blue
			break;

		default:
			llassert(false);
			return false;
	}

	return true;
}

void LLAvatarAppearance::setClothesColor(ETextureIndex te,
										 const LLColor4& new_color,
										 bool upload_bake)
{
	U32 param_name[3];
	if (teToColorParams(te, param_name))
	{
		setVisualParamWeight(param_name[0], new_color.mV[VX], upload_bake);
		setVisualParamWeight(param_name[1], new_color.mV[VY], upload_bake);
		setVisualParamWeight(param_name[2], new_color.mV[VZ], upload_bake);
	}
}

LLColor4 LLAvatarAppearance::getClothesColor(ETextureIndex te)
{
	LLColor4 color;
	U32 param_name[3];
	if (teToColorParams(te, param_name))
	{
		color.mV[VX] = getVisualParamWeight(param_name[0]);
		color.mV[VY] = getVisualParamWeight(param_name[1]);
		color.mV[VZ] = getVisualParamWeight(param_name[2]);
	}
	return color;
}

//static
LLColor4 LLAvatarAppearance::getDummyColor()
{
	return DUMMY_COLOR;
}

LLColor4 LLAvatarAppearance::getGlobalColor(const std::string& col_name) const
{
	if (col_name == "skin_color" && mTexSkinColor)
	{
		return mTexSkinColor->getColor();
	}
	else if (col_name == "hair_color" && mTexHairColor)
	{
		return mTexHairColor->getColor();
	}
	if (col_name == "eye_color" && mTexEyeColor)
	{
		return mTexEyeColor->getColor();
	}
	else
	{
		return LLColor4(0.f, 1.f, 1.f, 1.f); // good debugging color
	}
}

// Unlike most wearable functions, this works for both self and others.
//virtual
bool LLAvatarAppearance::isWearingWearableType(LLWearableType::EType t) const
{
	return mWearableData && mWearableData->getWearableCount(t) > 0;
}

LLTexLayerSet* LLAvatarAppearance::getAvatarLayerSet(EBakedTextureIndex i) const
{
	return mBakedTextureDatas[i].mTexLayerSet;
}

void LLAvatarAppearance::clearCollisionVolumes()
{
	std::for_each(mCollisionVolumes.begin(), mCollisionVolumes.end(),
				  DeletePointer());
	mCollisionVolumes.clear();
}

bool LLAvatarAppearance::allocateCollisionVolumes(U32 num)
{
	if (num != (U32)mCollisionVolumes.size())
	{
		clearCollisionVolumes();
		mCollisionVolumes.reserve(num);

		try
		{
			for (U32 i = 0; i < num; ++i)
			{
				LLAvatarJointCollisionVolume* cv =
					new LLAvatarJointCollisionVolume();
				mCollisionVolumes.push_back(cv);
			}
		}
		catch (...)
		{
			LLMemory::allocationFailed();
			llwarns << "Failed to allocate collision volumes" << llendl;
			clearCollisionVolumes();
			return false;
		}
	}

	return true;
}
