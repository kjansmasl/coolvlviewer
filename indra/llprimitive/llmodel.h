/**
 * @file llmodel.h
 * @brief Model handling class definitions
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLMODEL_H
#define LL_LLMODEL_H

#include <queue>

#include "llpointer.h"
#include "llrefcount.h"
#include "llvolume.h"
#include "llvector4.h"
#include "llmatrix4.h"

class daeElement;
class domMesh;

#define MAX_MODEL_FACES 8
// Fix for MAINT-6901, now reverted...
#define LL_NORMALIZE_ALL_MODELS 0

class LLMeshSkinInfo : public LLThreadSafeRefCount
{
public:
	LLMeshSkinInfo();
	LLMeshSkinInfo(const LLSD& data);
	LLMeshSkinInfo(const LLSD& data, const LLUUID& mesh_id);

	// Since LLMeshSkinInfo derives from LLThreadSafeRefCount, the default copy
	// constructor and operator are deleted, but we need (in the mesh model
	// upload floater) to clone a base LLMeshSkinInfo to LODs, so here is the
	// way to do it. HB
	void clone(const LLMeshSkinInfo& from);

	void fromLLSD(const LLSD& data);
	LLSD asLLSD(bool include_joints, bool lock_scale_if_joint_position) const;

	void updateHash(bool force = false);

public:
	LLUUID						mMeshID;
	LLMatrix4					mBindShapeMatrix;
	std::vector<std::string>	mJointNames;
	std::vector<U32>			mJointKeys;
	std::vector<LLMatrix4>		mInvBindMatrix;
	std::vector<LLMatrix4>		mAlternateBindMatrix;
	std::vector<LLMatrix4>		mInvBindShapeMatrix;
	U64							mHash;
	F32							mPelvisOffset;
	bool						mLockScaleIfJointPosition;
	bool						mInvalidJointsScrubbed;
};

class LLModel : public LLVolume
{
protected:
	LOG_CLASS(LLModel);

public:
	// Beware: LLModel::NUM_LODS != LLVolumeLODGroup::NUM_LODS but we must have
	// LLModel::LOD_HIGH < LLVolumeLODGroup::NUM_LODS, which is the case here.
	enum
	{
		LOD_IMPOSTOR = 0,
		LOD_LOW,
		LOD_MEDIUM,
		LOD_HIGH,
		LOD_PHYSICS,
		NUM_LODS
	};

	enum EModelStatus
	{
		NO_ERRORS = 0,
		VERTEX_NUMBER_OVERFLOW, //vertex number is >= 65535.
		BAD_ELEMENT,
		INVALID_STATUS
	};

	// hull_decomp is a vector of convex hulls; each convex hull is a set of
	// points.
	typedef std::vector<std::vector<LLVector3> > hull_decomp;
	typedef std::vector<LLVector3> hull;

	class PhysicsMesh
	{
	public:
		std::vector<LLVector3> mPositions;
		std::vector<LLVector3> mNormals;

		LL_INLINE void clear()
		{
			mPositions.clear();
			mNormals.clear();
		}

		LL_INLINE bool empty() const				{ return mPositions.empty(); }
	};

	class Decomposition
	{
	public:
		Decomposition()								{}
		Decomposition(const LLSD& data);
		Decomposition(const LLSD& data, const LLUUID& mesh_id);

		void fromLLSD(const LLSD& data);
		LLSD asLLSD() const;
		bool hasHullList() const;

		void merge(const Decomposition* rhs);

	public:
		LLUUID								mMeshID;
		LLModel::hull_decomp				mHull;
		LLModel::hull						mBaseHull;

		std::vector<LLModel::PhysicsMesh>	mMesh;
		LLModel::PhysicsMesh				mBaseHullMesh;
		LLModel::PhysicsMesh				mPhysicsShapeMesh;
	};

	LLModel(LLVolumeParams& params, F32 detail);
	~LLModel();

	std::string getName() const;

	bool loadModel(std::istream& is);
	bool loadSkinInfo(const LLSD& header, std::istream& is);
	bool loadDecomposition(const LLSD& header, std::istream& is);

	static LLSD writeModel(std::ostream& ostr, LLModel* physics, LLModel* high,
						   LLModel* medium, LLModel* low, LLModel* imposotr,
						   const LLModel::Decomposition& decomp,
						   bool upload_skin, bool upload_joints,
						   bool lock_scale_if_joint_position,
						   bool nowrite = false, bool as_slm = false,
						   S32 submodel_id = 0);

	static LLSD writeModelToStream(std::ostream& ostr, LLSD& mdl,
								   bool nowrite = false, bool as_slm = false);

	LL_INLINE void clearFacesAndMaterials()
	{
		mVolumeFaces.clear();
		mMaterialList.clear();
	}

	LL_INLINE EModelStatus getStatus() const		{ return mStatus; }
	static std::string getStatusString(U32 status);

	void setNumVolumeFaces(S32 count);
	void setVolumeFaceData(S32 f, LLStrider<LLVector3> pos,
						   LLStrider<LLVector3> norm, LLStrider<LLVector2> tc,
						   LLStrider<U16> ind, U32 num_verts, U32 num_indices);

	void generateNormals(F32 angle_cutoff);

	void addFace(const LLVolumeFace& face);

	void sortVolumeFacesByMaterialName();
	void normalizeVolumeFaces();
#if LL_NORMALIZE_ALL_MODELS
	static void normalizeModels(const std::vector<LLPointer<LLModel> >& model_list);
#endif
	void trimVolumeFacesToSize(U32 new_count = LL_SCULPT_MESH_MAX_FACES,
							   LLVolume::face_list_t* remainder = NULL);
	void remapVolumeFaces();
	void optimizeVolumeFaces();
	void offsetMesh(const LLVector3& pivotPoint);
	void getNormalizedScaleTranslation(LLVector3& scale_out,
									   LLVector3& translation_out);

	bool isMaterialListSubset(LLModel* ref);
#if 0	// Not used
	bool needToAddFaces(LLModel* ref, S32& ref_face_cnt, S32& mdl_face_cnt);
#endif
#if 0	// Moved to llfloatermodelpreview.cpp
	// Reorder face list based on mMaterialList in this and reference so order
	// matches that of reference (material ordering touchup)
	bool matchMaterialOrder(LLModel* ref, S32& ref_face_cnt,
							S32& mdl_face_cnt);
#endif

	typedef std::vector<std::string> material_list;
	LL_INLINE material_list& getMaterialList()		{ return mMaterialList; }

	// Data used for skin weights
	class JointWeight
	{
	public:
		JointWeight()
		:	mJointIdx(0),
			mWeight(0.f)
		{
		}

		JointWeight(S32 idx, F32 weight)
		:	mJointIdx(idx),
			mWeight(weight)
		{
		}

		LL_INLINE bool operator<(const JointWeight& rhs) const
		{
			if (mWeight == rhs.mWeight)
			{
				return mJointIdx < rhs.mJointIdx;
			}

			return mWeight < rhs.mWeight;
		}

	public:
		S32 mJointIdx;
		F32 mWeight;
	};

	struct CompareWeightGreater
	{
		LL_INLINE bool operator()(const JointWeight& lhs,
								  const JointWeight& rhs)
		{
			return rhs < lhs; // strongest = first
		}
	};

	// Returns false for values that are not within the tolerance for
	// equivalence
	LL_INLINE bool jointPositionalLookup(const LLVector3& a,
										 const LLVector3& b)
	{
		constexpr F32 epsilon = 1e-5f;
		return fabs(a[0] - b[0]) < epsilon && fabs(a[1] - b[1]) < epsilon &&
			   fabs(a[2] - b[2]) < epsilon;
	}

	// Gets the list of weight influences closest to given position
	typedef std::vector<JointWeight> weight_list;
	weight_list& getJointInfluences(const LLVector3& pos);

	void setConvexHullDecomposition(const hull_decomp& decomp);
	void updateHullCenters();

	// Used to be a validate_model(const LLModel* mdl) global function. HB
	bool validate(bool check_nans = false) const;

protected:
	void addVolumeFacesFromDomMesh(domMesh* mesh);

public:
	EModelStatus			mStatus;

	S32						mDecompID;			// Convex hull decomposition
	S32 					mLocalID;			// Id of model in its .slm file

	// A model/object can only have 8 faces, spillover faces will be moved to
	// new model/object and assigned a submodel id.
	S32						mSubmodelID;

	U32						mHullPoints;

	F32						mPelvisOffset;

	std::string				mRequestedLabel;	// Name requested in UI, if any
	std::string				mLabel;				// Name computed from dae

	material_list			mMaterialList;

	std::vector<LLVector3>	mHullCenter;

	// Copy of position array for this model -- mPosition[idx].mV[X, Y, Z]
	std::vector<LLVector3>	mPosition;

	// Map of positions to skin weights:
	// mSkinWeights[pos].mV[0..4] == <joint_index>.<weight>
	// joint_index corresponds to mJointList
	typedef std::map<LLVector3, weight_list> weight_map;
	weight_map				mSkinWeights;

	LLVector3				mNormalizedScale;
	LLVector3				mNormalizedTranslation;
	LLVector3				mCenterOfHullCenters;

	LLMeshSkinInfo			mSkinInfo;
	Decomposition			mPhysics;
};

typedef std::vector<LLPointer<LLModel> > model_list;
typedef std::queue<LLPointer<LLModel> >	model_queue;

class LLModelMaterialBase
{
public:
	LLModelMaterialBase()
	:	mFullbright(false),
		mDiffuseColor(1.f, 1.f, 1.f, 1.f)
	{
	}

public:
	LLColor4	mDiffuseColor;
	std::string	mDiffuseMapFilename;
	std::string	mDiffuseMapLabel;
	std::string	mBinding;
	bool		mFullbright;
};

class LLImportMaterial : public LLModelMaterialBase
{
    friend class LLMeshUploadThread;
    friend class LLModelPreview;

public:
    bool operator<(const LLImportMaterial& params) const;

    LLImportMaterial()
	:	LLModelMaterialBase(),
		mUserData(NULL)
    {
        mDiffuseColor.set(1, 1, 1, 1);
    }

    LLImportMaterial(const LLSD& data);

    LLSD asLLSD();

    LL_INLINE const LLUUID& getDiffuseMap() const	{ return mDiffuseMapID; }
    LL_INLINE void setDiffuseMap(const LLUUID& id)	{ mDiffuseMapID = id; }

protected:
    LLUUID	mDiffuseMapID;
	// Allows refs to viewer/platform-specific structs for each material
    // currently only stores an LLPointer<LLViewerFetchedTexture> > to
    // maintain refs to textures associated with each material for free
    // ref counting.
    void*	mUserData;
};

typedef std::map<std::string, LLImportMaterial> material_map;

class LLModelInstanceBase
{
public:
	LLModelInstanceBase(LLModel* model, LLMatrix4& transform,
						material_map& materials)
	:	mModel(model),
		mTransform(transform),
		mMaterial(materials)
	{
	}

	LLModelInstanceBase()
	:	mModel(NULL)
	{
	}

public:
	LLPointer<LLModel>	mModel;
	LLPointer<LLModel>	mLOD[5];
	LLUUID				mMeshID;
	LLMatrix4			mTransform;
	material_map		mMaterial;
};

class LLModelInstance : public LLModelInstanceBase
{
public:
	LLModelInstance(LLModel* model, const std::string& label,
					LLMatrix4& transform, material_map& materials)
	:	LLModelInstanceBase(model, transform, materials),
		mLabel(label),
		mLocalMeshID(-1)
	{
	}

	LLModelInstance(const LLSD& data);

	LLSD asLLSD();

public:
	LLUUID		mMeshID;
	S32			mLocalMeshID;
	std::string	mLabel;
};

#endif //LL_LLMODEL_H
