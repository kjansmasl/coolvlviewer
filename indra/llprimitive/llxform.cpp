/**
 * @file llxform.cpp
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

#include "llxform.h"

LLXform::LLXform() noexcept
{
	init();
}

void LLXform::init()
{
	mParent = NULL;
	mChanged = UNCHANGED;
	mPosition.clear();
	mRotation.loadIdentity();
	mScale.set(1.f, 1.f, 1.f);
	mWorldPosition.clear();
	mWorldRotation.loadIdentity();
	mIsAvatar = false;
	mScaleChildOffset = false;
}

// Link optimization: do not inline these llwarns
LL_NO_INLINE void LLXform::warn(U32 idx)
{
	const char* msg;
	switch (idx)
	{
		case 0:
			msg = "Non Finite in LLXform::setPosition(LLVector3)";
			break;

		case 1:
			msg = "Non Finite in LLXform::setPosition(F32, F32, F32)";
			break;

		case 2:
			msg = "Non Finite in LLXform::setPositionX(F32)";
			break;

		case 3:
			msg = "Non Finite in LLXform::setPositionY(F32)";
			break;

		case 4:
			msg = "Non Finite in LLXform::setPositionZ(F32)";
			break;

		case 5:
			msg = "Non Finite in LLXform::addPosition(LLVector3)";
			break;

		case 6:
			msg = "Non Finite in LLXform::setScale(LLVector3)";
			break;

		case 7:
			msg = "Non Finite in LLXform::setScale(F32, F32, F32)";
			break;

		case 8:
			msg = "Non Finite in LLXform::setRotation(LLQuaternion)";
			break;

		case 9:
			msg = "Non Finite in LLXform::setRotation(F32, F32, F32)";
			break;

		case 10:
			msg = "Non Finite in LLXform::setRotation(F32, F32, F32, F32)";
			break;

		default:
			msg = NULL;
			llerrs << "Please, update warn() messages..." << llendl;
	}
	llwarns << msg << llendl;
}

LLXform* LLXform::getRoot() const
{
	const LLXform* root = this;
	while (root->mParent)
	{
		root = root->mParent;
	}
	return (LLXform*)root;
}

LLXformMatrix::LLXformMatrix()
:	LLXform()
{
}

void LLXformMatrix::init()
{
	mWorldMatrix.setIdentity();
	mMin.clear();
	mMax.clear();

	LLXform::init();
}

void LLXformMatrix::update()
{
	if (mParent)
	{
		mWorldPosition = mPosition;
		if (mParent->getScaleChildOffset())
		{
			mWorldPosition.scaleVec(mParent->getScale());
		}
		mWorldPosition *= mParent->getWorldRotation();
		mWorldPosition += mParent->getWorldPosition();
		mWorldRotation = mRotation * mParent->getWorldRotation();
	}
	else
	{
		mWorldPosition = mPosition;
		mWorldRotation = mRotation;
	}
}

void LLXformMatrix::updateMatrix(bool update_bounds)
{
	update();

	mWorldMatrix.initAll(mScale, mWorldRotation, mWorldPosition);

	if (update_bounds && (mChanged & MOVED))
	{
		mMin.mV[0] = mMax.mV[0] = mWorldMatrix.mMatrix[3][0];
		mMin.mV[1] = mMax.mV[1] = mWorldMatrix.mMatrix[3][1];
		mMin.mV[2] = mMax.mV[2] = mWorldMatrix.mMatrix[3][2];

		F32 f0 = (fabs(mWorldMatrix.mMatrix[0][0]) +
				  fabs(mWorldMatrix.mMatrix[1][0]) +
				  fabs(mWorldMatrix.mMatrix[2][0])) * 0.5f;
		F32 f1 = (fabs(mWorldMatrix.mMatrix[0][1]) +
				  fabs(mWorldMatrix.mMatrix[1][1]) +
				  fabs(mWorldMatrix.mMatrix[2][1])) * 0.5f;
		F32 f2 = (fabs(mWorldMatrix.mMatrix[0][2]) +
				  fabs(mWorldMatrix.mMatrix[1][2]) +
				  fabs(mWorldMatrix.mMatrix[2][2])) * 0.5f;

		mMin.mV[0] -= f0;
		mMin.mV[1] -= f1;
		mMin.mV[2] -= f2;

		mMax.mV[0] += f0;
		mMax.mV[1] += f1;
		mMax.mV[2] += f2;
	}
}
