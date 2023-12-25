/**
 * @file LLAvatarJointMesh.cpp
 * @brief Implementation of LLAvatarJointMesh class
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

#include "llavatarjointmesh.h"

#include "lltexlayer.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "llrender.h"
#include "llmatrix3.h"
#include "llmatrix4.h"
#include "llvector4.h"

//-----------------------------------------------------------------------------
// LLSkinJoint class
//-----------------------------------------------------------------------------

LLSkinJoint::LLSkinJoint()
{
	mJoint = NULL;
}

LLSkinJoint::~LLSkinJoint()
{
	mJoint = NULL;
}

//static
LLAvatarJoint* LLSkinJoint::getBaseSkeletonAncestor(LLAvatarJoint* joint)
{
    LLJoint* ancestor = joint->getParent();
    while (ancestor && ancestor->getParent() &&
		   ancestor->getSupport() != LLJoint::SUPPORT_BASE)
    {
        LL_DEBUGS("Avatar") << "skipping non-base ancestor: "
							<< ancestor->getName() << LL_ENDL;
        ancestor = ancestor->getParent();
    }
    return (LLAvatarJoint*)ancestor;
}

//static
LLVector3 LLSkinJoint::totalSkinOffset(LLAvatarJoint* joint)
{
    LLVector3 total_offset;
    while (joint)
    {
		if (joint->getSupport() == LLJoint::SUPPORT_BASE)
		{
	        total_offset += joint->getSkinOffset();
		}
		joint = (LLAvatarJoint*)joint->getParent();
    }
    return total_offset;
}

bool LLSkinJoint::setupSkinJoint(LLAvatarJoint* joint)
{
	mRootToJointSkinOffset.clear();

	// find the named joint
	mJoint = joint;
	if (!mJoint)
	{
		mRootToParentJointSkinOffset.clear();
		llwarns << "NULL joint !" << llendl;
		return false;
	}

	// compute the inverse root skin matrix
	mRootToJointSkinOffset = -totalSkinOffset(joint);

	mRootToParentJointSkinOffset = -totalSkinOffset(getBaseSkeletonAncestor(joint));
	
	return true;
}

//-----------------------------------------------------------------------------
// LLAvatarJointMesh class
//-----------------------------------------------------------------------------

U32 LLAvatarJointMesh::sClothingMaskImageName = 0;
LLColor4 LLAvatarJointMesh::sClothingInnerColor;

LLAvatarJointMesh::LLAvatarJointMesh()
:	mTexture(NULL),
	mLayerSet(NULL),
	mTestImageName(0),
	mFaceIndexCount(0),
	mColor(LLColor4(1.f, 1.f, 1.f, 1.f)),
	mShiny(0.f),
#if 0
	mCullBackFaces(true),
#endif
	mFace(NULL),
	mMesh(NULL),
	mMeshID(0),
	mSkinJoints(NULL),
	mNumSkinJoints(0)
{
	mUpdateXform = false;
	mValid = false;
	mIsTransparent = false;
}

LLAvatarJointMesh::~LLAvatarJointMesh()
{
	mMesh = NULL;
	mTexture = NULL;
	freeSkinData();
}

bool LLAvatarJointMesh::allocateSkinData(U32 numSkinJoints)
{
	mSkinJoints = new LLSkinJoint[numSkinJoints];
	mNumSkinJoints = numSkinJoints;
	return true;
}

void LLAvatarJointMesh::freeSkinData()
{
	mNumSkinJoints = 0;
	if (mSkinJoints)
	{
		delete[] mSkinJoints;
		mSkinJoints = NULL;
	}
}

void LLAvatarJointMesh::getColor(F32* red, F32* green, F32* blue, F32* alpha)
{
	*red   = mColor[0];
	*green = mColor[1];
	*blue  = mColor[2];
	*alpha = mColor[3];
}

void LLAvatarJointMesh::setColor(F32 red, F32 green, F32 blue, F32 alpha)
{
	mColor[0] = red;
	mColor[1] = green;
	mColor[2] = blue;
	mColor[3] = alpha;
}

void LLAvatarJointMesh::setColor(const LLColor4& color)
{
	mColor = color;
}

void LLAvatarJointMesh::setTexture(LLGLTexture* texture)
{
	mTexture = texture;

	// texture and dynamic_texture are mutually exclusive
	if (texture)
	{
		mLayerSet = NULL;
#if 0
		texture->bindTexture(0);
		texture->setClamp(true, true);
#endif
	}
}

bool LLAvatarJointMesh::hasGLTexture() const
{
	return mTexture.notNull() && mTexture->hasGLTexture();
}

// Sets the shape texture (takes precedence over normal texture)
void LLAvatarJointMesh::setLayerSet(LLTexLayerSet* layer_set)
{
	mLayerSet = layer_set;

	// texture and dynamic_texture are mutually exclusive
	if (layer_set)
	{
		mTexture = NULL;
	}
}

bool LLAvatarJointMesh::hasComposite() const
{
	return mLayerSet && mLayerSet->hasComposite();
}

void LLAvatarJointMesh::setMesh(LLPolyMesh* mesh)
{
	// Set the mesh pointer
	mMesh = mesh;

	// Release any existing skin joints
	freeSkinData();

	if (!mMesh)
	{
		return;
	}

	// Acquire the transform from the mesh object
	setPosition(mMesh->getPosition());
	setRotation(mMesh->getRotation());
	setScale(mMesh->getScale());

	// Create skin joints if necessary
	if (mMesh->hasWeights() && !mMesh->isLOD())
	{
		U32 num_joint_names = mMesh->getNumJointNames();

		allocateSkinData(num_joint_names);

		std::string* joint_names = mMesh->getJointNames();
		for (U32 i = 0; i < num_joint_names; ++i)
		{
			const std::string& name = joint_names[i];
			LLJoint* jointp = getRoot()->findAliasedJoint(name);
			if (jointp)
			{
				LLAvatarJoint* avjointp = jointp->asAvatarJoint();
				if (avjointp)
				{
					mSkinJoints[i].setupSkinJoint(avjointp);
					continue;
				}
			}
			llwarns << "Root joint for '" << name 
					<< "' is not an avatar joint !" << llendl;
		}
	}

	// Setup joint array
	if (!mMesh->isLOD())
	{
		LL_DEBUGS("Avatar") << getName() << " joint render entries: "
							<< mMesh->mJointRenderData.size() << LL_ENDL;
		LLAvatarJoint* avjointp = getRoot()->asAvatarJoint();
		if (avjointp)
		{
			setupJoint(avjointp);
		}
		else
		{
			llwarns << "Root joint is not an avatar joint !" << llendl;
		}
	}
}

void LLAvatarJointMesh::setupJoint(LLAvatarJoint* current_joint)
{
	for (U32 sj = 0; sj < mNumSkinJoints; ++sj)
	{
		LLSkinJoint& js = mSkinJoints[sj];
		if (js.mJoint != current_joint)
		{
			continue;
		}

		// We have found a skinjoint for this joint...
		LL_DEBUGS("Avatar") << "Mesh: " << getName() << " joint "
							<< current_joint->getName()
							<< " matches skinjoint " << sj << LL_ENDL;

		// Is the last joint in the array our parent ?
		// SL-287: we need to update this so that the results are the same if
		// additional extended-skeleton joints lay between this joint and the
		// original parent.
		LLJoint* ancestor = LLSkinJoint::getBaseSkeletonAncestor(current_joint);
		if (!ancestor)
		{
			llwarns << "cannot find an ancestor joint for: "
					<< current_joint->getName() << ". Aborted." << llendl;
			continue;
		}
		if (!mMesh)
		{
			llwarns << "mMesh is NULL for joint: " << current_joint->getName()
					<< ". Aborted." << llendl;
			continue;
		}

		std::vector<LLJointRenderData*>& jrd = mMesh->mJointRenderData;
		if (jrd.size() &&
			jrd.back()->mWorldMatrix == &ancestor->getWorldMatrix())
		{
			// ...then just add ourselves
			LL_DEBUGS("Avatar") << "adding joint #" << jrd.size() << ": "
								<< js.mJoint->getName() << LL_ENDL;
			LLJoint* jointp = js.mJoint;
			jrd.push_back(new LLJointRenderData(&jointp->getWorldMatrix(),
												&js));
		}
		else
		{
			// ...otherwise add our parent and ourselves
			LL_DEBUGS("Avatar") << "adding ancestor joint #" << jrd.size()
								<< ": " << ancestor->getName() << LL_ENDL;
			jrd.push_back(new LLJointRenderData(&ancestor->getWorldMatrix(),
												NULL));
			LL_DEBUGS("Avatar") << "adding joint #" << jrd.size() << ": "
								<< current_joint->getName() << LL_ENDL;
			jrd.push_back(new LLJointRenderData(&current_joint->getWorldMatrix(),
												&js));
		}
	}

	// Depth-first traversal
	for (S32 i = 0, count = current_joint->mChildren.size(); i < count; ++i)
	{
		LLJoint* jointp = current_joint->mChildren[i];
		if (jointp)	// Paranoia
		{
			LLAvatarJoint* avjointp = jointp->asAvatarJoint();
			if (avjointp)
			{
				setupJoint(avjointp);
			}
		}
	}
}
