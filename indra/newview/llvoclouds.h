/**
 * @file llvoclouds.h
 * @brief Description of LLVOClouds class
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

#ifndef LL_LLVOCLOUDS_H
#define LL_LLVOCLOUDS_H

#include "llcolor4u.h"

#include "llviewerobject.h"

class LLCloudGroup;
class LLViewerCloudGroup;
class LLViewerTexture;

class LLVOClouds final : public LLAlphaObject
{
public:
	LLVOClouds(const LLUUID& id, LLViewerRegion* regionp);

	// Initialize data that's only inited once per class.
	static void initClass();

	void updateDrawable(bool force_damped) override;

	LLDrawable* createDrawable() override;

	bool updateGeometry(LLDrawable* drawable) override;

	void getGeometry(S32 idx, LLStrider<LLVector4a>& verticesp,
					 LLStrider<LLVector3>& normalsp,
					 LLStrider<LLVector2>& texcoordsp,
					 LLStrider<LLColor4U>& colorsp,
					 LLStrider<LLColor4U>& emissivep,
					 LLStrider<U16>& indicesp) override;

	// Whether this object needs to do an idleUpdate:
	LL_INLINE bool isActive() const override			{ return true; }

	LL_INLINE void updateFaceSize(S32 idx) override 	{}

	F32 getPartSize(S32 idx) override;

	void updateTextures() override;

	// Generates accurate apparent angle and area:
	void setPixelAreaAndAngle() override;

	void idleUpdate(F64 time) override;

	U32 getPartitionType() const override;

	LL_INLINE void setCloudGroup(LLCloudGroup* cgp)		{ mCloudGroupp = cgp; }

protected:
	~LLVOClouds() override = default;

protected:
	LLCloudGroup* mCloudGroupp;
};

extern LLUUID gCloudTextureID;

#endif // LL_VO_CLOUDS_H
