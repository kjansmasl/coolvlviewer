/**
 * @file llmodelloader.cpp
 * @brief LLModelLoader class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
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

#include "linden_common.h"

#include "llmodelloader.h"

#include "llapp.h"
#include "llcallbacklist.h"
#include "llmatrix4a.h"
#include "lljoint.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lltimer.h"			// For ms_sleep()

std::list<LLModelLoader*> LLModelLoader::sActiveLoaderList;

void stretch_extents(LLModel* model, const LLMatrix4a& mat, LLVector4a& min,
					 LLVector4a& max, bool& first_transform)
{
	static const LLVector4a box[] =
	{
		LLVector4a(-1.f, 1.f, -1.f),
		LLVector4a(-1.f, 1.f, 1.f),
		LLVector4a(-1.f, -1.f, -1.f),
		LLVector4a(-1.f, -1.f, 1.f),
		LLVector4a(1.f, 1.f, -1.f),
		LLVector4a(1.f, 1.f, 1.f),
		LLVector4a(1.f, -1.f, -1.f),
		LLVector4a(1.f, -1.f, 1.f),
	};

	for (S32 j = 0; j < model->getNumVolumeFaces(); ++j)
	{
		const LLVolumeFace& face = model->getVolumeFace(j);

		LLVector4a center;
		center.setAdd(face.mExtents[0], face.mExtents[1]);
		center.mul(0.5f);
		LLVector4a size;
		size.setSub(face.mExtents[1],face.mExtents[0]);
		size.mul(0.5f);

		for (U32 i = 0; i < 8; ++i)
		{
			LLVector4a t;
			t.setMul(size, box[i]);
			t.add(center);

			LLVector4a v;

			mat.affineTransform(t, v);

			if (first_transform)
			{
				first_transform = false;
				min = max = v;
			}
			else
			{
				update_min_max(min, max, v);
			}
		}
	}
}

void stretch_extents(LLModel* model, const LLMatrix4& mat, LLVector3& min,
					 LLVector3& max, bool& first_transform)
{
	LLVector4a mina, maxa;
	LLMatrix4a mata;
	mata.loadu(mat);
	mina.load3(min.mV);
	maxa.load3(max.mV);

	stretch_extents(model, mata, mina, maxa, first_transform);

	min.set(mina.getF32ptr());
	max.set(maxa.getF32ptr());
}

LLModelLoader::LLModelLoader(const std::string& filename, S32 lod,
							 load_callback_t load_cb,
							 joint_lookup_func_t joint_lookup_func,
							 texture_load_func_t texture_load_func,
							 state_callback_t state_cb,
							 void* userdata,
							 JointTransformMap& joint_transform_map,
							 JointNameSet& joints_from_nodes,
							 JointMap& legal_joint_names,
							 U32 max_joints_per_mesh)
:	mJointList(joint_transform_map),
	mJointsFromNode(joints_from_nodes),
	LLThread("Model Loader"),
	mFilename(filename),
	mLod(lod),
	mFirstTransform(true),
	mNumOfFetchingTextures(0),
	mLoadCallback(load_cb),
	mJointLookupFunc(joint_lookup_func),
	mTextureLoadFunc(texture_load_func),
	mStateCallback(state_cb),
	mUserData(userdata),
	mNoNormalize(false),
	mNoOptimize(false),
	mCacheOnlyHitIfRigged(false),
	mTrySLM(false),
	mRigValidJointUpload(true),
	mLegacyRigFlags(0),
	mWarningsArray(LLSD::emptyArray()),
	mMaxJointsPerMesh(max_joints_per_mesh),
	mJointMap(legal_joint_names)
{
	LLStringUtil::replaceString(mFilename, "#", "%23");
	assert_main_thread();
	sActiveLoaderList.push_back(this);
}

LLModelLoader::~LLModelLoader()
{
	assert_main_thread();
	sActiveLoaderList.remove(this);
}

void LLModelLoader::run()
{
	mWarningsArray.clear();
	doLoadModel();
	doOnIdleOneTime(boost::bind(&LLModelLoader::loadModelCallback, this));
}

//static
bool LLModelLoader::getSLMFilename(const std::string& model_filename,
								   std::string& slm_filename)
{
	slm_filename = model_filename;

	size_t i = model_filename.rfind(".");
	if (i != std::string::npos && i > 0)
	{
		slm_filename.resize(i, '\0');
		slm_filename.append(".slm");
		return true;
	}

	return false;
}

bool LLModelLoader::doLoadModel()
{
	if (mTrySLM)
	{
		// First, look for a .slm file of the same name that was modified later
		// than the specified model file
		std::string slm_filename;
		if (getSLMFilename(mFilename, slm_filename))
		{
			time_t slm_time = LLFile::lastModidied(slm_filename);
			if (slm_time)	// If .slm file exists
			{
				time_t model_time = LLFile::lastModidied(mFilename);
				if (!model_time || model_time < slm_time)
				{
					// If this fails, fall through and try loading from the
					// model file
					if (loadFromSLM(slm_filename))
					{
						// Successfully loading from an slm implicitly sets all
						// LoDs
						mLod = -1;
						return true;
					}
				}
			}
		}
	}
	return openFile(mFilename);
}

void LLModelLoader::setLoadState(U32 state)
{
	if (mStateCallback)
	{
		mStateCallback(state, mUserData);
	}
}

bool LLModelLoader::loadFromSLM(const std::string& filename)
{
	// Only need to populate mScene with data from slm
	llstat stat;
	if (LLFile::stat(filename, &stat))
	{
		// File does not exist
		return false;
	}
	S32 file_size = (S32)stat.st_size;

	llifstream stream(filename.c_str(),
					  std::ifstream::in | std::ifstream::binary);
	if (!stream.is_open())
	{
		llwarns << "Could not open file '" << filename << "' for reading."
				<< llendl;
		return false;
	}

	LLSD data;
	LLSDSerialize::fromBinary(data, stream, file_size);
	stream.close();

	// Build model list for each LoD
	model_list model[LLModel::NUM_LODS];

	if (data["version"].asInteger() != SLM_SUPPORTED_VERSION)
	{
		// unsupported version
		return false;
	}

	LLSD& mesh = data["mesh"];

	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);

	for (S32 lod = 0; lod < LLModel::NUM_LODS; ++lod)
	{
		for (U32 i = 0, count = mesh.size(); i < count; ++i)
		{
			std::stringstream str(mesh[i].asString());
			LLPointer<LLModel> loaded_model = new LLModel(volume_params,
														  (F32)lod);
			if (loaded_model->loadModel(str))
			{
				loaded_model->mLocalID = i;
				model[lod].emplace_back(loaded_model);

				if (lod == LLModel::LOD_HIGH)
				{
					if (!loaded_model->mSkinInfo.mJointNames.empty())
					{
						// Check to see if rig is valid
						critiqueRigForUploadApplicability(loaded_model->mSkinInfo.mJointNames);
					}
					else if (mCacheOnlyHitIfRigged)
					{
						return false;
					}
				}
			}
		}
	}

	if (model[LLModel::LOD_HIGH].empty())
	{
		// Failed to load high lod
		return false;
	}

	// Set name.
	std::string name = data["name"];
	if (!name.empty())
	{
		model[LLModel::LOD_HIGH][0]->mLabel = name;
	}

	// Load instance list
	model_instance_list_t instances;

	LLSD& instance = data["instance"];

	for (U32 i = 0, count = instance.size(); i < count; ++i)
	{
		// Deserialize instance list
		instances.emplace_back(instance[i]);

		// Match up model instance pointers
		S32 idx = instances[i].mLocalMeshID;
		std::string instance_label = instances[i].mLabel;

		for (U32 lod = 0; lod < LLModel::NUM_LODS; ++lod)
		{
			if (!model[lod].empty())
			{
				S32 lod_size = model[lod].size();
				if (idx >= lod_size)
				{
					if (lod_size)
					{
						instances[i].mLOD[lod] = model[lod][0];
					}
					else
					{
						instances[i].mLOD[lod] = NULL;
					}
					continue;
				}

				if (model[lod][idx] && model[lod][idx]->mLabel.empty() &&
					!instance_label.empty())
				{
					// restore model names
					std::string name = instance_label;
					switch (lod)
					{
						case LLModel::LOD_IMPOSTOR:	name += "_LOD0"; break;
						case LLModel::LOD_LOW:		name += "_LOD1"; break;
						case LLModel::LOD_MEDIUM:	name += "_LOD2"; break;
						case LLModel::LOD_PHYSICS:	name += "_PHYS"; break;
						case LLModel::LOD_HIGH:						 break;
					}
					model[lod][idx]->mLabel = name;
				}

				instances[i].mLOD[lod] = model[lod][idx];
			}
		}

		if (!instances[i].mModel)
		{
			instances[i].mModel = model[LLModel::LOD_HIGH][idx];
		}
	}

	// Convert instances to mScene
	mFirstTransform = true;
	for (U32 i = 0, count = instances.size(); i < count; ++i)
	{
		LLModelInstance& cur_instance = instances[i];
		mScene[cur_instance.mTransform].emplace_back(cur_instance);
		stretch_extents(cur_instance.mModel, cur_instance.mTransform,
						mExtents[0], mExtents[1], mFirstTransform);
	}

	setLoadState(DONE);

	return true;
}

//static
bool LLModelLoader::isAlive(LLModelLoader* loader)
{
	if (!loader)
	{
		return false;
	}

	std::list<LLModelLoader*>::iterator iter = sActiveLoaderList.begin();
	std::list<LLModelLoader*>::iterator end = sActiveLoaderList.end();
	for ( ; iter != end && *iter != loader; ++iter) ;

	return *iter == loader;
}

void LLModelLoader::loadModelCallback()
{
	if (!LLApp::isExiting() && mLoadCallback)
	{
		mLoadCallback(mScene, mModelList ,mLod, mUserData);
	}

	// Wait until this thread is stopped before deleting self
	while (!isStopped())
	{
		ms_sleep(10);
	}

	// Double check if "this" is valid before deleting it, in case it is
	// aborted while running.
	if (!isAlive(this))
	{
		return;
	}

	delete this;
}

void LLModelLoader::critiqueRigForUploadApplicability(const std::vector<std::string>& joints)
{
	// Determines the following use cases for a rig:
	// 1. It is suitable for upload with skin weights & joint positions, or
	// 2. It is suitable for upload as standard av with just skin weights
	// It's OK that both could end up being true. Both start out as true and
	// are forced to false if any mesh in the model file is not vald by that
	// criterion. Note that a file can contain multiple meshes.
	mLegacyRigFlags |= determineRigLegacyFlags(joints);
}

U32 LLModelLoader::determineRigLegacyFlags(const std::vector<std::string>& joints)
{
	U32 count = joints.size();
	if (count == 0)
	{
		// No joints in asset
		LLSD args;
		args["Message"] = "NoJoint";
		mWarningsArray.append(args);
		return LEGACY_RIG_FLAG_NO_JOINT;
	}
	if (count > mMaxJointsPerMesh)
	{
		// Too many joints in asset
		llwarns << "Rigged to " << count << " joints, while maximum is "
				<< mMaxJointsPerMesh << ". Skinning disabled." << llendl;
		LLSD args;
		args["Message"] = "TooManyJoint";
		args["JOINTS"] = LLSD::Integer(count);
		args["MAX"] = LLSD::Integer(mMaxJointsPerMesh);
		mWarningsArray.append(args);
		return LEGACY_RIG_FLAG_TOO_MANY_JOINTS;
	}

	U32 unknown_joint_count = 0;
	for (U32 i = 0; i < count; ++i)
	{
		const std::string& name = joints[i];
		if (!mJointMap.count(name))
		{
			llwarns << "Rigged to unrecognized joint name: " << name << llendl;
			LLSD args;
			args["Message"] = "UnrecognizedJoint";
			args["NAME"] = name;
			mWarningsArray.append(args);
			++unknown_joint_count;
		}
	}
	if (unknown_joint_count)
	{
		llwarns << "Skinning disabled due to unknown joints." << llendl;
		LLSD args;
		args["Message"] = "UnknownJoints";
		args["COUNT"] = LLSD::Integer(unknown_joint_count);
		mWarningsArray.append(args);		
		return LEGACY_RIG_FLAG_UNKNOWN_JOINT;
	}

	return 0;	// All OK !
}

// Called in the main thread
void LLModelLoader::loadTextures()
{
	if (!mTextureLoadFunc) return;

	bool is_paused = isPaused();
	pause(); //pause the loader

	for (scene::iterator iter = mScene.begin(), end = mScene.end();
		 iter != end; ++iter)
	{
		for (U32 i = 0, count = iter->second.size(); i < count; ++i)
		{
			for (std::map<std::string, LLImportMaterial>::iterator
					j = iter->second[i].mMaterial.begin(),
					end2 = iter->second[i].mMaterial.end();
				 j != end2; ++j)
			{
				LLImportMaterial& material = j->second;

				if (!material.mDiffuseMapFilename.empty())
				{
					mNumOfFetchingTextures += mTextureLoadFunc(material,
															   mUserData);
				}
			}
		}
	}

	if (!is_paused)
	{
		unpause();
	}
}
