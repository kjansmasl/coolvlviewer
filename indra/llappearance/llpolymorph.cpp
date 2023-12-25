/**
 * @file llpolymorph.cpp
 * @brief Implementation of LLPolyMesh class
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

#include "llpolymorph.h"

#include "llavatarappearance.h"
#include "llavatarjoint.h"
#include "lldate.h"
#include "llendianswizzle.h"
#include "llfasttimer.h"
#include "llpolymesh.h"
#include "llvolume.h"
#include "llwearable.h"
#include "llxmltree.h"

constexpr F32 NORMAL_SOFTEN_FACTOR = 0.65f;

//-----------------------------------------------------------------------------
// LLPolyMorphData() class
//-----------------------------------------------------------------------------

LLPolyMorphData::LLPolyMorphData(const char* morph_name)
:	mName(morph_name),
	mNumIndices(0),
	mCurrentIndex(0),
	mTotalDistortion(0.f),
	mMaxDistortion(0.f),
	mVertexIndices(NULL),
	mCoords(NULL),
	mNormals(NULL),
	mBinormals(NULL),
	mTexCoords(NULL),
	mMesh(NULL),
	mSuccessfullyAllocated(true)
{
	mAvgDistortion.clear();
}

LLPolyMorphData::LLPolyMorphData(const LLPolyMorphData& rhs)
:	mName(rhs.mName),
	mNumIndices(rhs.mNumIndices),
	mCurrentIndex(0),
	mTotalDistortion(rhs.mTotalDistortion),
	mAvgDistortion(rhs.mAvgDistortion),
	mMaxDistortion(rhs.mMaxDistortion),
	mVertexIndices(NULL),
	mCoords(NULL),
	mNormals(NULL),
	mBinormals(NULL),
	mTexCoords(NULL),
	mMesh(NULL),
	mSuccessfullyAllocated(false)
{
	const S32 num_verts = mNumIndices;

	S32 vert_size = sizeof(LLVector4a) * num_verts;
	mCoords = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mCoords)
	{
		freeData();
		return;
	}

	mNormals = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mNormals)
	{
		freeData();
		return;
	}

	mBinormals = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mBinormals)
	{
		freeData();
		return;
	}

	S32 tex_size = (num_verts * sizeof(LLVector2) + 0xF) & ~0xF;
	mTexCoords = (LLVector2*)allocate_volume_mem(tex_size);
	if (!mTexCoords)
	{
		freeData();
		return;
	}

	mVertexIndices = (U32*)allocate_volume_mem(num_verts * sizeof(U32));
	if (!mVertexIndices)
	{
		freeData();
		return;
	}

	mSuccessfullyAllocated = true;

	for (S32 v = 0; v < num_verts; ++v)
	{
		mCoords[v] = rhs.mCoords[v];
		mNormals[v] = rhs.mNormals[v];
		mBinormals[v] = rhs.mBinormals[v];
		mTexCoords[v] = rhs.mTexCoords[v];
		mVertexIndices[v] = rhs.mVertexIndices[v];
	}
}

LLPolyMorphData::~LLPolyMorphData()
{
	freeData();
}

bool LLPolyMorphData::loadBinary(LLFILE* fp, LLPolyMeshSharedData* mesh)
{
	S32 num_verts;
	S32 num_read = fread(&num_verts, sizeof(S32), 1, fp);
	llendianswizzle(&num_verts, sizeof(S32), 1);
	if (num_read != 1)
	{
		llwarns << "Cannot read number of morph target vertices" << llendl;
		return false;
	}

	//-------------------------------------------------------------------------
	// Free any existing data
	//-------------------------------------------------------------------------
	freeData();

	//-------------------------------------------------------------------------
	// Allocate vertices
	//-------------------------------------------------------------------------
	S32 vert_size = sizeof(LLVector4a) * num_verts;

	mCoords = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mCoords)
	{
		freeData();
		return false;
	}

	mNormals = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mNormals)
	{
		freeData();
		return false;
	}

	mBinormals = (LLVector4a*)allocate_volume_mem(vert_size);
	if (!mBinormals)
	{
		freeData();
		return false;
	}

	S32 tex_size = (num_verts * sizeof(LLVector2) + 0xF) & ~0xF;
	mTexCoords = (LLVector2*)allocate_volume_mem(tex_size);
	if (!mTexCoords)
	{
		freeData();
		return false;
	}

	// Actually, we are allocating more space than we need for the skiplist
	mVertexIndices = (U32*)allocate_volume_mem(num_verts * sizeof(U32));
	if (!mVertexIndices)
	{
		freeData();
		return false;
	}

	mNumIndices = 0;
	mTotalDistortion = 0.f;
	mMaxDistortion = 0.f;
	mAvgDistortion.clear();
	mMesh = mesh;

	//-------------------------------------------------------------------------
	// Read vertices
	//-------------------------------------------------------------------------
	for (S32 v = 0; v < num_verts; ++v)
	{
		num_read = fread(&mVertexIndices[v], sizeof(U32), 1, fp);
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (num_read != 1)
		{
			llwarns << "Cannot read morph target vertex number" << llendl;
			return false;
		}

		if (mVertexIndices[v] > 10000)
		{
			llwarns << "Bad morph index: " << mVertexIndices[v] << llendl;
			llassert(false);
			return false;
		}

		num_read = fread(&mCoords[v], sizeof(F32), 3, fp);
		llendianswizzle(&mCoords[v], sizeof(F32), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read morph target vertex coordinates" << llendl;
			return false;
		}

		F32 magnitude = mCoords[v].getLength3().getF32();

		mTotalDistortion += magnitude;
		LLVector4a t;
		t.setAbs(mCoords[v]);
		mAvgDistortion.add(t);

		if (magnitude > mMaxDistortion)
		{
			mMaxDistortion = magnitude;
		}

		num_read = fread(&mNormals[v], sizeof(F32), 3, fp);
		llendianswizzle(&mNormals[v], sizeof(F32), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read morph target normal" << llendl;
			return false;
		}

		num_read = fread(&mBinormals[v], sizeof(F32), 3, fp);
		llendianswizzle(&mBinormals[v], sizeof(F32), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read morph target binormal" << llendl;
			return false;
		}

		num_read = fread(&mTexCoords[v].mV, sizeof(F32), 2, fp);
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (num_read != 2)
		{
			llwarns << "west_limit read morph target uv" << llendl;
			return false;
		}

		++mNumIndices;
	}

	mAvgDistortion.mul(1.f / (F32)mNumIndices);
	mAvgDistortion.normalize3fast();

	return true;
}

void LLPolyMorphData::freeData()
{
	if (mCoords)
	{
		free_volume_mem(mCoords);
		mCoords = NULL;
	}

	if (mNormals)
	{
		free_volume_mem(mNormals);
		mNormals = NULL;
	}

	if (mBinormals)
	{
		free_volume_mem(mBinormals);
		mBinormals = NULL;
	}

	if (mTexCoords)
	{
		free_volume_mem(mTexCoords);
		mTexCoords = NULL;
	}

	if (mVertexIndices)
	{
		free_volume_mem(mVertexIndices);
		mVertexIndices = NULL;
	}
}

//-----------------------------------------------------------------------------
// LLPolyMorphTargetInfo() class
//-----------------------------------------------------------------------------

LLPolyMorphTargetInfo::LLPolyMorphTargetInfo()
:	mIsClothingMorph(false)
{
}

bool LLPolyMorphTargetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("param") && node->getChildByName("param_morph"));

	if (!LLViewerVisualParamInfo::parseXml(node))
	{
		return false;
	}

	// Get mixed-case name
	static LLStdStringHandle name_string =
		LLXmlTree::addAttributeString("name");
	if (!node->getFastAttributeString(name_string, mMorphName))
	{
		llwarns << "Avatar file: <param> is missing name attribute" << llendl;
		return false;  // Continue, ignoring this tag
	}

	static LLStdStringHandle clothing_morph_string =
		LLXmlTree::addAttributeString("clothing_morph");
	node->getFastAttributeBool(clothing_morph_string, mIsClothingMorph);

	LLXmlTreeNode* paramNode = node->getChildByName("param_morph");
	if (!paramNode)
	{
		llwarns << "Failed to getChildByName(\"param_morph\")" << llendl;
		return false;
	}

	for (LLXmlTreeNode* child_node = paramNode->getFirstChild();
		 child_node; child_node = paramNode->getNextChild())
	{
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		if (child_node->hasName("volume_morph"))
		{
			std::string volume_name;
			if (child_node->getFastAttributeString(name_string, volume_name))
			{
				LLVector3 scale;
				static LLStdStringHandle scale_string =
					LLXmlTree::addAttributeString("scale");
				child_node->getFastAttributeVector3(scale_string, scale);

				LLVector3 pos;
				static LLStdStringHandle pos_string =
					LLXmlTree::addAttributeString("pos");
				child_node->getFastAttributeVector3(pos_string, pos);

				mVolumeInfoList.emplace_back(volume_name, scale,pos);
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTarget() class
//-----------------------------------------------------------------------------

LLPolyMorphTarget::LLPolyMorphTarget(LLPolyMesh* poly_mesh)
:	LLViewerVisualParam(),
	mMorphData(NULL),
	mMesh(poly_mesh),
	mVertMask(NULL),
	mLastSex(SEX_FEMALE),
	mNumMorphMasksPending(0),
	mVolumeMorphs()
{
}

LLPolyMorphTarget::LLPolyMorphTarget(const LLPolyMorphTarget& other)
:	LLViewerVisualParam(other),
	mMorphData(other.mMorphData),
	mMesh(other.mMesh),
	mVertMask(other.mVertMask ? new LLPolyVertexMask(*other.mVertMask) : NULL),
	mLastSex(other.mLastSex),
	mNumMorphMasksPending(other.mNumMorphMasksPending),
	mVolumeMorphs(other.mVolumeMorphs)
{
}

LLPolyMorphTarget::~LLPolyMorphTarget()
{
	if (mVertMask)
	{
		delete mVertMask;
		mVertMask = NULL;
	}
}

bool LLPolyMorphTarget::setInfo(LLPolyMorphTargetInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
	{
		return false;
	}
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), false);

	LLAvatarAppearance* avatarp = mMesh->getAvatar();
	if (!avatarp)
	{
		llwarns << "NULL avatar for this morph target !" << llendl;
		return false;
	}

	for (LLPolyMorphTargetInfo::volume_info_list_t::iterator
			iter = getInfo()->mVolumeInfoList.begin(),
			end = getInfo()->mVolumeInfoList.end();
		 iter != end; ++iter)
	{
		LLPolyVolumeMorphInfo* volume_info = &(*iter);
		for (S32 i = 0, count = avatarp->mCollisionVolumes.size();
			 i < count; ++i)
		{
			if (avatarp->mCollisionVolumes[i]->getName() == volume_info->mName)
			{
				mVolumeMorphs.emplace_back(avatarp->mCollisionVolumes[i],
										   volume_info->mScale,
										   volume_info->mPos);
				break;
			}
		}
	}

	std::string morph_param_name = getInfo()->mMorphName;
	mMorphData = mMesh->getMorphData(morph_param_name);
	if (!mMorphData)
	{
		const std::string driven_tag = "_Driven";
		U32 pos = morph_param_name.find(driven_tag);
		if (pos > 0)
		{
			morph_param_name = morph_param_name.substr(0, pos);
			mMorphData = mMesh->getMorphData(morph_param_name);
		}
	}
	if (!mMorphData)
	{
		llwarns << "No morph target named " << morph_param_name
				<< " found in mesh." << llendl;
		return false;  // Continue, ignoring this tag
	}
	return true;
}

//virtual
LLViewerVisualParam* LLPolyMorphTarget::cloneParam(LLWearable* wearable) const
{
	return new LLPolyMorphTarget(*this);
}

#if 0	// Unused methods
LLVector4a LLPolyMorphTarget::getVertexDistortion(S32 requested_index,
												  LLPolyMesh* mesh)
{
	if (!mMorphData || mMesh != mesh) return LLVector4a::getZero();

	for (U32 index = 0, count = mMorphData->mNumIndices; index < count;
		 ++index)
	{
		if (mMorphData->mVertexIndices[index] == (U32)requested_index)
		{
			return mMorphData->mCoords[index];
		}
	}

	return LLVector4a::getZero();
}

const LLVector4a* LLPolyMorphTarget::getFirstDistortion(U32* index,
														LLPolyMesh** poly_mesh)
{
	if (!mMorphData)
	{
		return &LLVector4a::getZero();
	}

	mMorphData->mCurrentIndex = 0;

	if (!mMorphData->mNumIndices)
	{
		return NULL;
	}

	LLVector4a* result = &mMorphData->mCoords[mMorphData->mCurrentIndex];
	if (index)
	{
		*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
	}
	if (poly_mesh)
	{
		*poly_mesh = mMesh;
	}
	return result;
}

const LLVector4a* LLPolyMorphTarget::getNextDistortion(U32* index,
													   LLPolyMesh** poly_mesh)
{
	if (!mMorphData)
	{
		return &LLVector4a::getZero();
	}

	if (++mMorphData->mCurrentIndex >= mMorphData->mNumIndices)
	{
		return NULL;
	}

	LLVector4a* result = &mMorphData->mCoords[mMorphData->mCurrentIndex];
	if (index)
	{
		*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
	}
	if (poly_mesh)
	{
		*poly_mesh = mMesh;
	}
	return result;
}

F32	LLPolyMorphTarget::getTotalDistortion()
{
	return mMorphData ? mMorphData->mTotalDistortion : 0.f;
}

const LLVector4a& LLPolyMorphTarget::getAvgDistortion()
{
	return mMorphData ? mMorphData->mAvgDistortion : LLVector4a::getZero();
}

F32	LLPolyMorphTarget::getMaxDistortion()
{
	return mMorphData ? mMorphData->mMaxDistortion : 0.f;
}
#endif

void LLPolyMorphTarget::apply(ESex avatar_sex)
{
	if (!mMorphData || mNumMorphMasksPending > 0)
	{
		return;
	}

	LL_FAST_TIMER(FTM_APPLY_MORPH_TARGET);

	mLastSex = avatar_sex;

	// Check for NaN condition (NaN is detected if a variable doesn't equal
	// itself.
	if (mCurWeight != mCurWeight)
	{
		mCurWeight = 0.f;
	}
	if (mLastWeight != mLastWeight)
	{
		mLastWeight = mCurWeight + .001f;
	}

	// Perform differential update of morph
	F32 delta_weight = (getSex() & avatar_sex) ? mCurWeight - mLastWeight
											   : getDefaultWeight() - mLastWeight;
	// Store last weight
	mLastWeight += delta_weight;

	if (delta_weight != 0.f)
	{
		llassert(!mMesh->isLOD());
		LLVector4a* coords = mMesh->getWritableCoords();

		LLVector4a* scaled_normals = mMesh->getScaledNormals();
		LLVector4a* normals = mMesh->getWritableNormals();

		LLVector4a* scaled_binormals = mMesh->getScaledBinormals();
		LLVector4a* binormals = mMesh->getWritableBinormals();

		LLVector4a* clothing_weights = mMesh->getWritableClothingWeights();
		LLVector2* tex_coords = mMesh->getWritableTexCoords();

		F32* maskWeightArray = mVertMask ? mVertMask->getMorphMaskWeights()
										 : NULL;

		for (U32 vert_index_morph = 0, count = mMorphData->mNumIndices;
			 vert_index_morph < count; ++vert_index_morph)
		{
			S32 vert_index_mesh = mMorphData->mVertexIndices[vert_index_morph];

			F32 maskWeight = 1.f;
			if (maskWeightArray)
			{
				maskWeight = maskWeightArray[vert_index_morph];
			}

			LLVector4a pos = mMorphData->mCoords[vert_index_morph];
			pos.mul(delta_weight * maskWeight);
			coords[vert_index_mesh].add(pos);

			if (getInfo()->mIsClothingMorph && clothing_weights)
			{
				LLVector4a clothing_offset = mMorphData->mCoords[vert_index_morph];
				clothing_offset.mul(delta_weight * maskWeight);
				LLVector4a* clothing_weight = &clothing_weights[vert_index_mesh];
				clothing_weight->add(clothing_offset);
				clothing_weight->getF32ptr()[VW] = maskWeight;
			}

			// Calculate new normals based on half angles
			LLVector4a norm = mMorphData->mNormals[vert_index_morph];
			norm.mul(delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR);
			scaled_normals[vert_index_mesh].add(norm);
			norm = scaled_normals[vert_index_mesh];

			// Guard against degenerate input data before we create NaNs below !
			norm.normalize3fast();
			normals[vert_index_mesh] = norm;

			// Calculate new binormals
			LLVector4a binorm = mMorphData->mBinormals[vert_index_morph];

			// Guard against degenerate input data before we create NaNs below !
			if (!binorm.isFinite3() ||
				binorm.dot3(binorm).getF32() <= F_APPROXIMATELY_ZERO)
			{
				binorm.set(1.f, 0.f, 0.f, 1.f);
			}

			binorm.mul(delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR);
			scaled_binormals[vert_index_mesh].add(binorm);
			LLVector4a tangent;
			tangent.setCross3(scaled_binormals[vert_index_mesh], norm);
			LLVector4a& normalized_binormal = binormals[vert_index_mesh];

			normalized_binormal.setCross3(norm, tangent);
			normalized_binormal.normalize3fast();

			tex_coords[vert_index_mesh] += mMorphData->mTexCoords[vert_index_morph] *
										   delta_weight * maskWeight;
		}

		// Now apply volume changes
		applyVolumeChanges(delta_weight);
	}

	if (mNext)
	{
		mNext->apply(avatar_sex);
	}
}

void LLPolyMorphTarget::applyMask(U8* mask_tex_data, S32 width, S32 height,
								  S32 num_components, bool invert)
{
	LLVector4a* clothing_weights =
		getInfo()->mIsClothingMorph ? mMesh->getWritableClothingWeights()
									: NULL;

	if (!mVertMask)
	{
		mVertMask = new LLPolyVertexMask(mMorphData);
		--mNumMorphMasksPending;
	}
	else
	{
		// Remove effect of previous mask
		F32* mask_weights = mVertMask ? mVertMask->getMorphMaskWeights() : NULL;
		if (mask_weights)
		{
			LLVector4a* coords = mMesh->getWritableCoords();
			LLVector4a* scaled_normals = mMesh->getScaledNormals();
			LLVector4a* scaled_binormals = mMesh->getScaledBinormals();
			LLVector2* tex_coords = mMesh->getWritableTexCoords();

			LLVector4Logical clothing_mask;
			clothing_mask.clear();
			clothing_mask.setElement<0>();
			clothing_mask.setElement<1>();
			clothing_mask.setElement<2>();

			for (U32 vert = 0, count = mMorphData->mNumIndices; vert < count;
				 ++vert)
			{
				F32 last_weight = mLastWeight * mask_weights[vert];
				S32 out_vert = mMorphData->mVertexIndices[vert];

				// Remove effect of existing masked morph
				LLVector4a t;
				t = mMorphData->mCoords[vert];
				t.mul(last_weight);
				coords[out_vert].sub(t);

				t = mMorphData->mNormals[vert];
				t.mul(last_weight * NORMAL_SOFTEN_FACTOR);
				scaled_normals[out_vert].sub(t);

				t = mMorphData->mBinormals[vert];
				t.mul(last_weight * NORMAL_SOFTEN_FACTOR);
				scaled_binormals[out_vert].sub(t);

				tex_coords[out_vert] -= mMorphData->mTexCoords[vert] *
										last_weight;

				if (clothing_weights)
				{
					LLVector4a clothing_offset = mMorphData->mCoords[vert];
					clothing_offset.mul(last_weight);
					LLVector4a* clothing_weight = &clothing_weights[out_vert];
					LLVector4a t;
					t.setSub(*clothing_weight, clothing_offset);
					clothing_weight->setSelectWithMask(clothing_mask, t,
													   *clothing_weight);
				}
			}
		}
	}

	// Set last weight to 0, since we've removed the effect of this morph
	mLastWeight = 0.f;

	mVertMask->generateMask(mask_tex_data, width, height, num_components,
							invert, clothing_weights);

	apply(mLastSex);
}

void LLPolyMorphTarget::applyVolumeChanges(F32 delta_weight)
{
	// Apply volume changes
	for (volume_list_t::iterator iter = mVolumeMorphs.begin(),
								 end = mVolumeMorphs.end();
		 iter != end; ++iter)
	{
		LLPolyVolumeMorph* morph = &(*iter);
		if (!morph) continue;	// Paranoia

		LLVector3 scale_delta = morph->mScale * delta_weight;
		LLVector3 pos_delta = morph->mPos * delta_weight;

		morph->mVolume->setScale(morph->mVolume->getScale() + scale_delta);
		morph->mVolume->setPosition(morph->mVolume->getPosition() + pos_delta);
	}
}

//-----------------------------------------------------------------------------
// LLPolyVertexMask() class
//-----------------------------------------------------------------------------

LLPolyVertexMask::LLPolyVertexMask(LLPolyMorphData* morph_data)
:	mMorphData(NULL),
	mWeightsGenerated(false)
{
	mWeights = (F32*)allocate_volume_mem(morph_data->mNumIndices *
										 sizeof(F32));
	if (mWeights)
	{
		mMorphData = morph_data;
	}
	else
	{
		llwarns << "Failure to allocate memory for weights !" << llendl;
	}
}

LLPolyVertexMask::LLPolyVertexMask(const LLPolyVertexMask& other)
:	mWeights(NULL),
	mMorphData(other.mMorphData),
	mWeightsGenerated(other.mWeightsGenerated)
{
	if (mMorphData && mMorphData->mNumIndices > 0)
	{
		mWeights = (F32*)allocate_volume_mem(other.mMorphData->mNumIndices *
											 sizeof(F32));
		if (mWeights)
		{
			memcpy(mWeights, other.mWeights,
				   sizeof(F32) * mMorphData->mNumIndices);
		}
		else
		{
			llwarns << "Failure to allocate memory for weights !" << llendl;
		}
	}
	else
	{
		llwarns << "Invalid morph data !" << llendl;
		llassert(false);
	}
}

LLPolyVertexMask::~LLPolyVertexMask()
{
	if (mWeights)
	{
		free_volume_mem(mWeights);
		mWeights = NULL;
	}
}

void LLPolyVertexMask::generateMask(U8* mask_tex_data, S32 width, S32 height,
									S32 num_components, bool invert,
									LLVector4a* clothing_weights)
{
	LLVector2 uv_coords;
	for (U32 index = 0, count = mMorphData->mNumIndices; index < count;
		 ++index)
	{
		S32 vert_idx = mMorphData->mVertexIndices[index];

		const S32* shared_vert_idx =
			mMorphData->mMesh->getSharedVert(vert_idx);
		if (shared_vert_idx)
		{
			uv_coords = mMorphData->mMesh->getUVs(*shared_vert_idx);
		}
		else
		{
			uv_coords = mMorphData->mMesh->getUVs(vert_idx);
		}
		U32 s = llclamp((U32)(uv_coords.mV[VX] * (F32)(width - 1)), 0U,
						(U32)width - 1);
		U32 t = llclamp((U32)(uv_coords.mV[VY] * (F32)(height - 1)), 0U,
						(U32)height - 1);

		F32 weight;
		if (mask_tex_data)
		{
			weight = (F32)mask_tex_data[(t * width + s) * num_components +
										num_components - 1] / 255.f;
		}
		else
		{
			weight = 0.f;
		}
		if (invert)
		{
			weight = 1.f - weight;
		}
		mWeights[index] = weight;
#if 0
		// Now apply step function
		mWeights[index] = mWeights[index] > 0.95f ? 1.f : 0.f;
#endif
		if (clothing_weights)
		{
			clothing_weights[vert_idx].getF32ptr()[VW] = mWeights[index];
		}
	}
	mWeightsGenerated = true;
}

F32* LLPolyVertexMask::getMorphMaskWeights()
{
	return mWeightsGenerated ? mWeights : NULL;
}
