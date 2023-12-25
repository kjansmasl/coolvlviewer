/**
 * @file llvograss.h
 * @brief Description of LLVOGrass class
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

#ifndef LL_LLVOGRASS_H
#define LL_LLVOGRASS_H

#include <map>

#include "llviewerobject.h"

class LLSelectNode;
class LLSurfacePatch;
class LLViewerTexture;

class LLVOGrass final : public LLAlphaObject
{
protected:
	LOG_CLASS(LLVOGrass);

	~LLVOGrass() override = default;

public:
	LLVOGrass(const LLUUID& id, LLViewerRegion* regionp);

	// Initialize data that is only initialized once per class.
	static void initClass();
	static void cleanupClass();

	U32 getPartitionType() const override;

	U32 processUpdateMessage(LLMessageSystem* mesgsys, void** user_data,
							 U32 block_num, EObjectUpdateType upd_type,
							 LLDataPacker* dp) override;
	static void import(LLFILE* file, LLMessageSystem* mesgsys,
					   const LLVector3& pos);
	void exportFile(LLFILE* file, const LLVector3& position);

	void updateDrawable(bool force_damped) override;

	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;
	void getGeometry(S32 idx, LLStrider<LLVector4a>& verticesp,
					 LLStrider<LLVector3>& normalsp,
					 LLStrider<LLVector2>& texcoordsp,
					 LLStrider<LLColor4U>& colorsp,
					 LLStrider<LLColor4U>& emissivep,
					 LLStrider<U16>& indicesp) override;

	LL_INLINE void updateFaceSize(S32 idx) override		{}
	void updateTextures() override;
	bool updateLOD() override;

	// Generate accurate apparent angle and area
	void setPixelAreaAndAngle() override;

	void plantBlades();

	// Whether this object needs to do an idleUpdate:
	LL_INLINE bool isActive() const override			{ return true; }

	void idleUpdate(F64 time) override;

	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  // Which face to check, -1 = ALL_SIDES
							  S32 face = -1,
							  bool pick_transparent = false,
							  bool pick_rigged = false,
							  // Which face was hit
							  S32* face_hit = NULL,
							  // Intersection point
							  LLVector4a* intersection = NULL,
							  // Texture coordinates of the intersection point
							  LLVector2* tex_coord = NULL,
							  // Surface normal at the intersection point
							  LLVector4a* normal = NULL,
							  // Surface tangent at the intersection point
							  LLVector4a* tangent = NULL) override;

	void generateSilhouette(LLSelectNode* nodep);

private:
	void generateSilhouetteVertices(std::vector<LLVector3> &vertices,
									std::vector<LLVector3> &normals,
									const LLVector3& view_vec,
									const LLMatrix4& mat,
									const LLMatrix3& norm_mat);
	void updateSpecies();

public:
	struct GrassSpeciesData
	{
		LLUUID				mTextureID;

		F32					mBladeSizeX;
		F32					mBladeSizeY;
	};

	U64						mLastPatchUpdateTime;

	F32						mBladeSizeX;
	F32						mBladeSizeY;
	F32						mBWAOverlap;

	LLSurfacePatch*			mPatch;		//  Stores the land patch where the grass is centered

	U8						mSpecies;	// Species of grass

	static S32				sMaxGrassSpecies;

	typedef std::map<std::string, S32> species_list_t;
	static species_list_t	sSpeciesNames;

private:
	F32						mLastHeight;		// For cheap update hack
	S32						mNumBlades;

	typedef std::map<U32, GrassSpeciesData*> data_map_t;
	static data_map_t		sSpeciesTable;
};

#endif // LL_VO_GRASS_
