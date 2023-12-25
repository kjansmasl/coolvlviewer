/**
 * @file llxform.h
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

#ifndef LL_XFORM_H
#define LL_XFORM_H

#include "llvector3.h"
#include "llmatrix4.h"
#include "llvolume.h"		// For allocate_volume_mem() & free_volume_mem()

// MAX_OBJECT_Z should match REGION_HEIGHT_METERS:
constexpr F32 MAX_OBJECT_Z						= 4096.f;
constexpr F32 MIN_OBJECT_Z						= -256.f;
constexpr F32 DEFAULT_MAX_PRIM_SCALE			= 64.f;
constexpr F32 DEFAULT_MAX_PRIM_SCALE_NO_MESH	= 10.f;
constexpr F32 DEFAULT_MIN_PRIM_SCALE			= 0.01f;

class LLXform
{
protected:
	LOG_CLASS(LLXform);

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}
#endif

	typedef enum e_changed_flags
	{
		UNCHANGED  	= 0x00,
		TRANSLATED 	= 0x01,
		ROTATED		= 0x02,
		SCALED		= 0x04,
		SHIFTED		= 0x08,
		GEOMETRY	= 0x10,
		TEXTURE		= 0x20,
		MOVED       = TRANSLATED | ROTATED | SCALED,
		SILHOUETTE	= 0x40,
		ALL_CHANGED = 0x7f
	} EChangedFlags;

	void init();

	LLXform() noexcept;
	virtual ~LLXform() = default;

	LL_INLINE void getLocalMat4(LLMatrix4& mat) const
	{
		mat.initAll(mScale, mRotation, mPosition);
	}

	LL_INLINE bool setParent(LLXform* parent);

	LL_INLINE void setPosition(const LLVector3& pos);
	LL_INLINE void setPosition(F32 x, F32 y, F32 z);
	LL_INLINE void setPositionX(F32 x);
	LL_INLINE void setPositionY(F32 y);
	LL_INLINE void setPositionZ(F32 z);
	LL_INLINE void addPosition(const LLVector3& pos);

	LL_INLINE void setScale(const LLVector3& scale);
	LL_INLINE void setScale(F32 x, F32 y, F32 z);
	LL_INLINE void setRotation(const LLQuaternion& rot);
	LL_INLINE void setRotation(F32 x, F32 y, F32 z);
	LL_INLINE void setRotation(F32 x, F32 y, F32 z, F32 s);

	// Above functions must be inline for speed, but also need to emit
	// warnings. llwarns causes inline LLError::CallSite static objects that
	// make more work for the linker, and duplicating the warning string
	// everywhere is costly. Avoid inline llwarns by calling this function. HB
	LL_NO_INLINE void warn(U32 idx);

	LL_INLINE void setChanged(U32 bits)						{ mChanged |= bits; }
	LL_INLINE bool isChanged() const						{ return mChanged != 0; }
	LL_INLINE bool isChanged(U32 bits) const				{ return (mChanged & bits) != 0; }
	LL_INLINE void clearChanged()							{ mChanged = 0; }
	LL_INLINE void clearChanged(U32 bits)					{ mChanged &= ~bits; }

	LL_INLINE void setScaleChildOffset(bool scale)			{ mScaleChildOffset = scale; }
	LL_INLINE bool getScaleChildOffset()					{ return mScaleChildOffset; }

	LL_INLINE LLXform* getParent() const					{ return mParent; }
	LLXform* getRoot() const;

	LL_INLINE virtual bool isRoot() const					{ return !mParent; }
	LL_INLINE virtual bool isRootEdit() const				{ return !mParent; }

	LL_INLINE const LLVector3& getPosition() const			{ return mPosition; }
	LL_INLINE const LLVector3& getScale() const				{ return mScale; }
	LL_INLINE const LLQuaternion& getRotation() const		{ return mRotation; }
	LL_INLINE const LLVector3& getPositionW() const			{ return mWorldPosition; }
	LL_INLINE const LLQuaternion& getWorldRotation() const	{ return mWorldRotation; }
	LL_INLINE const LLVector3& getWorldPosition() const		{ return mWorldPosition; }

	LL_INLINE bool isAvatar() const							{ return mIsAvatar; }

protected:
	LL_INLINE void setAvatar(bool b)						{ mIsAvatar = b; }

protected:
	LLQuaternion  mRotation;
	LLVector3	  mPosition;
	LLVector3	  mScale;

	// *TODO: move these world transform members to LLXformMatrix as they are
	//        *never* updated or accessed in the base class.
	LLVector3	  mWorldPosition;
	LLQuaternion  mWorldRotation;

	LLXform*      mParent;
	U32			  mChanged;

	bool		  mIsAvatar;
	bool		  mScaleChildOffset;
};

class LLXformMatrix : public LLXform
{
public:
	LLXformMatrix();

	LL_INLINE const LLMatrix4& getWorldMatrix() const		{ return mWorldMatrix; }
	LL_INLINE void setWorldMatrix(const LLMatrix4& mat)		{ mWorldMatrix = mat; }

	void init();

	void update();
	void updateMatrix(bool update_bounds = true);

	LL_INLINE void getMinMax(LLVector3& min, LLVector3& max) const
	{
		min = mMin;
		max = mMax;
	}

protected:
	LLMatrix4	mWorldMatrix;
	LLVector3	mMin;
	LLVector3	mMax;
};

LL_INLINE bool LLXform::setParent(LLXform* parent)
{
	// Validate and make sure we are not creating a loop
	if (parent == mParent)
	{
		return true;
	}
	if (parent)
	{
		LLXform* cur_par = parent->mParent;
		while (cur_par)
		{
			if (cur_par == this)
			{
				return false;
			}
			cur_par = cur_par->mParent;
		}
	}
	mParent = parent;
	return true;
}

LL_INLINE void LLXform::setPosition(const LLVector3& pos)
{
	mChanged |= TRANSLATED;
	if (pos.isFinite())
	{
		mPosition = pos;
	}
	else
	{
		mPosition.clear();
		warn(0);
	}
}

LL_INLINE void LLXform::setPosition(F32 x, F32 y, F32 z)
{
	mChanged |= TRANSLATED;
	if (llfinite(x) && llfinite(y) && llfinite(z))
	{
		mPosition.set(x, y, z);
	}
	else
	{
		mPosition.clear();
		warn(1);
	}
}

LL_INLINE void LLXform::setPositionX(F32 x)
{
	mChanged |= TRANSLATED;
	if (llfinite(x))
	{
		mPosition.mV[VX] = x;
	}
	else
	{
		mPosition.mV[VX] = 0.f;
		warn(2);
	}
}

LL_INLINE void LLXform::setPositionY(F32 y)
{
	mChanged |= TRANSLATED;
	if (llfinite(y))
	{
		mPosition.mV[VY] = y;
	}
	else
	{
		mPosition.mV[VY] = 0.f;
		warn(3);
	}
}

LL_INLINE void LLXform::setPositionZ(F32 z)
{
	mChanged |= TRANSLATED;
	if (llfinite(z))
	{
		mPosition.mV[VZ] = z;
	}
	else
	{
		mPosition.mV[VZ] = 0.f;
		warn(4);
	}
}

LL_INLINE void LLXform::addPosition(const LLVector3& pos)
{
	if (pos.isFinite())
	{
		mChanged |= TRANSLATED;
		mPosition += pos;
	}
	else
	{
		warn(5);
	}
}

LL_INLINE void LLXform::setScale(const LLVector3& scale)
{
	mChanged |= SCALED;
	if (scale.isFinite())
	{
		mScale = scale;
	}
	else
	{
		mScale.set(1.f, 1.f, 1.f);
		warn(6);
	}
}

LL_INLINE void LLXform::setScale(F32 x, F32 y, F32 z)
{
	mChanged |= SCALED;
	if (llfinite(x) && llfinite(y) && llfinite(z))
	{
		mScale.set(x, y, z);
	}
	else
	{
		mScale.set(1.f, 1.f, 1.f);
		warn(7);
	}
}

LL_INLINE void LLXform::setRotation(const LLQuaternion& rot)
{
	mChanged |= ROTATED;
	if (rot.isFinite())
	{
		mRotation = rot;
	}
	else
	{
		mRotation.loadIdentity();
		warn(8);
	}
}

LL_INLINE void LLXform::setRotation(F32 x, F32 y, F32 z)
{
	mChanged |= ROTATED;
	if (llfinite(x) && llfinite(y) && llfinite(z))
	{
		mRotation.setEulerAngles(x, y, z);
	}
	else
	{
		mRotation.loadIdentity();
		warn(9);
	}
}

LL_INLINE void LLXform::setRotation(F32 x, F32 y, F32 z, F32 s)
{
	mChanged |= ROTATED;
	if (llfinite(x) && llfinite(y) && llfinite(z) && llfinite(s))
	{
		mRotation.mQ[VX] = x;
		mRotation.mQ[VY] = y;
		mRotation.mQ[VZ] = z;
		mRotation.mQ[VS] = s;
	}
	else
	{
		mRotation.loadIdentity();
		warn(10);
	}
}

#endif
