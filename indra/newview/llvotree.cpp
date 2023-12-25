/**
 * @file llvotree.cpp
 * @brief LLVOTree class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llvotree.h"

#include "imageids.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llnoise.h"
#include "llnotifications.h"
#include "llprimitive.h"
#include "lltree_common.h"
#include "llxmltree.h"
#include "object_flags.h"
#include "llraytrace.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llworld.h"

constexpr S32 MAX_SLICES	= 32;
constexpr F32 LEAF_LEFT		= 0.52f;
constexpr F32 LEAF_RIGHT	= 0.98f;
constexpr F32 LEAF_TOP		= 1.0f;
constexpr F32 LEAF_BOTTOM	= 0.52f;
constexpr F32 LEAF_WIDTH	= 1.f;
// How many frames between wind update per tree:
constexpr U32 FRAMES_PER_WIND_UPDATE = 20U;

S32 LLVOTree::sLODVertexOffset[MAX_NUM_TREE_LOD_LEVELS];
S32 LLVOTree::sLODVertexCount[MAX_NUM_TREE_LOD_LEVELS];
S32 LLVOTree::sLODIndexOffset[MAX_NUM_TREE_LOD_LEVELS];
S32 LLVOTree::sLODIndexCount[MAX_NUM_TREE_LOD_LEVELS];
S32 LLVOTree::sLODSlices[MAX_NUM_TREE_LOD_LEVELS] = { 10, 5, 4, 3 };
F32 LLVOTree::sLODAngles[MAX_NUM_TREE_LOD_LEVELS] = { 30.f, 20.f, 15.f, F_ALMOST_ZERO };

F32 LLVOTree::sTreeAnimationDamping = 0.99f;
F32 LLVOTree::sTreeTrunkStiffness = 0.1f;
F32 LLVOTree::sTreeWindSensitivity = 0.005f;
bool LLVOTree::sRenderAnimateTrees = false;

F32 LLVOTree::sTreeFactor = 1.f;

LLVOTree::data_map_t LLVOTree::sSpeciesTable;
LLVOTree::species_list_t LLVOTree::sSpeciesNames;
S32 LLVOTree::sMaxTreeSpecies = 0;

///////////////////////////////////////////////////////////////////////////////
// LLTreePartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLTreePartition::LLTreePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(0, false, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_TREE;
	mPartitionType = LLViewerRegion::PARTITION_TREE;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}

///////////////////////////////////////////////////////////////////////////////
// LLVOTree class
///////////////////////////////////////////////////////////////////////////////

LLVOTree::LLVOTree(const LLUUID& id, LLViewerRegion* regionp)
:	LLViewerObject(id, LL_PCODE_LEGACY_TREE, regionp),
	mTrunkLOD(0),
	mFrameCount(0),
	mWind(mRegionp->mWind.getVelocity(getPositionRegion()))
{
	data_map_t::const_iterator it = sSpeciesTable.begin();
	mSpecies = it->first;
	mSpeciesData = it->second;
}

LLVOTree::~LLVOTree()
{
	if (mData)
	{
		delete[] mData;
		mData = NULL;
	}
	mReferenceBuffer = NULL;
	mUpdateMeshBuffer = NULL;
}

//static
bool LLVOTree::isTreeRenderingStopped()
{
	return sTreeFactor < sLODAngles[MAX_NUM_TREE_LOD_LEVELS - 1];
}

// static
void LLVOTree::initClass()
{
	updateSettings();

	std::string xml_filename =
		gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "trees.xml");

	LLXmlTree tree_def_tree;
	if (!tree_def_tree.parseFile(xml_filename))
	{
		llerrs << "Failed to parse tree file." << llendl;
	}

	LLXmlTreeNode* rootp = tree_def_tree.getRoot();
	if (!rootp)
	{
		llerrs << "Failed to parse tree file." << llendl;
		return;
	}

	LLUUID id;
	S32 s32_val, species;
	F32 f32_val;
	for (LLXmlTreeNode* tree_def = rootp->getFirstChild(); tree_def;
		 tree_def = rootp->getNextChild())
	{
		if (!tree_def->hasName("tree"))
		{
			llwarns << "Invalid tree definition node \"" << tree_def->getName()
					<< "\"" << llendl;
			continue;
		}

		bool success = true;

		static LLStdStringHandle species_id_string = LLXmlTree::addAttributeString("species_id");
		if (!tree_def->getFastAttributeS32(species_id_string, species))
		{
			llwarns << "No species id defined" << llendl;
			continue;
		}

		if (species < 0)
		{
			llwarns << "Invalid species id " << species << llendl;
			continue;
		}

		if (sSpeciesTable.count(species))
		{
			llwarns << "Tree species \"" << species
					<< "\" already defined !  Duplicate discarded." << llendl;
			continue;
		}

		TreeSpeciesData* new_tree = new TreeSpeciesData();

		static LLStdStringHandle texture_id_string =
			LLXmlTree::addAttributeString("texture_id");
		success &= tree_def->getFastAttributeUUID(texture_id_string, id);
		new_tree->mTextureID = id;

		static LLStdStringHandle droop_string =
			LLXmlTree::addAttributeString("droop");
		success &= tree_def->getFastAttributeF32(droop_string, f32_val);
		new_tree->mDroop = f32_val;

		static LLStdStringHandle twist_string =
			LLXmlTree::addAttributeString("twist");
		success &= tree_def->getFastAttributeF32(twist_string, f32_val);
		new_tree->mTwist = f32_val;

		static LLStdStringHandle branches_string =
			LLXmlTree::addAttributeString("branches");
		success &= tree_def->getFastAttributeF32(branches_string, f32_val);
		new_tree->mBranches = f32_val;

		static LLStdStringHandle depth_string =
			LLXmlTree::addAttributeString("depth");
		success &= tree_def->getFastAttributeS32(depth_string, s32_val);
		new_tree->mDepth = s32_val;

		static LLStdStringHandle scale_step_string =
			LLXmlTree::addAttributeString("scale_step");
		success &= tree_def->getFastAttributeF32(scale_step_string, f32_val);
		new_tree->mScaleStep = f32_val;

		static LLStdStringHandle trunk_depth_string =
			LLXmlTree::addAttributeString("trunk_depth");
		success &= tree_def->getFastAttributeS32(trunk_depth_string, s32_val);
		new_tree->mTrunkDepth = s32_val;

		static LLStdStringHandle branch_length_string =
			LLXmlTree::addAttributeString("branch_length");
		success &= tree_def->getFastAttributeF32(branch_length_string,
												 f32_val);
		new_tree->mBranchLength = f32_val;

		static LLStdStringHandle trunk_length_string =
			LLXmlTree::addAttributeString("trunk_length");
		success &= tree_def->getFastAttributeF32(trunk_length_string, f32_val);
		new_tree->mTrunkLength = f32_val;

		static LLStdStringHandle leaf_scale_string =
			LLXmlTree::addAttributeString("leaf_scale");
		success &= tree_def->getFastAttributeF32(leaf_scale_string, f32_val);
		new_tree->mLeafScale = f32_val;

		static LLStdStringHandle billboard_scale_string =
			LLXmlTree::addAttributeString("billboard_scale");
		success &= tree_def->getFastAttributeF32(billboard_scale_string,
												 f32_val);
		new_tree->mBillboardScale = f32_val;

		static LLStdStringHandle billboard_ratio_string =
			LLXmlTree::addAttributeString("billboard_ratio");
		success &= tree_def->getFastAttributeF32(billboard_ratio_string,
												 f32_val);
		new_tree->mBillboardRatio = f32_val;

		static LLStdStringHandle trunk_aspect_string =
			LLXmlTree::addAttributeString("trunk_aspect");
		success &= tree_def->getFastAttributeF32(trunk_aspect_string, f32_val);
		new_tree->mTrunkAspect = f32_val;

		static LLStdStringHandle branch_aspect_string =
			LLXmlTree::addAttributeString("branch_aspect");
		success &= tree_def->getFastAttributeF32(branch_aspect_string,
												 f32_val);
		new_tree->mBranchAspect = f32_val;

		static LLStdStringHandle leaf_rotate_string =
			LLXmlTree::addAttributeString("leaf_rotate");
		success &= tree_def->getFastAttributeF32(leaf_rotate_string, f32_val);
		new_tree->mRandomLeafRotate = f32_val;

		static LLStdStringHandle noise_mag_string =
			LLXmlTree::addAttributeString("noise_mag");
		success &= tree_def->getFastAttributeF32(noise_mag_string, f32_val);
		new_tree->mNoiseMag = f32_val;

		static LLStdStringHandle noise_scale_string =
			LLXmlTree::addAttributeString("noise_scale");
		success &= tree_def->getFastAttributeF32(noise_scale_string, f32_val);
		new_tree->mNoiseScale = f32_val;

		static LLStdStringHandle taper_string =
			LLXmlTree::addAttributeString("taper");
		success &= tree_def->getFastAttributeF32(taper_string, f32_val);
		new_tree->mTaper = f32_val;

		static LLStdStringHandle repeat_z_string =
			LLXmlTree::addAttributeString("repeat_z");
		success &= tree_def->getFastAttributeF32(repeat_z_string, f32_val);
		new_tree->mRepeatTrunkZ = f32_val;

		sSpeciesTable[species] = new_tree;

		if (species >= sMaxTreeSpecies)
		{
			sMaxTreeSpecies = species + 1;
		}

		std::string name;
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		success &= tree_def->getFastAttributeString(name_string, name);
		sSpeciesNames[name] = species;

		if (!success)
		{
			llwarns << "Incomplete definition of tree " << name << llendl;
		}
	}

	if (sSpeciesTable.empty())
	{
		llerrs << "Could not load any tree species !" << llendl;
	}

	bool have_all_trees = true;
	std::string err;

	for (S32 i = 0; i < sMaxTreeSpecies; ++i)
	{
		if (!sSpeciesTable.count(i))
		{
			err.append(llformat(" %d", i));
			have_all_trees = false;
		}
	}

	if (!have_all_trees)
	{
		LLSD args;
		args["SPECIES"] = err;
		gNotifications.add("ErrorUndefinedTrees", args);
	}
}

//static
void LLVOTree::cleanupClass()
{
	std::for_each(sSpeciesTable.begin(), sSpeciesTable.end(),
				  DeletePairedPointer());
	sSpeciesTable.clear();
}

//static
void LLVOTree::updateSettings()
{
	sTreeFactor = llmax(gSavedSettings.getF32("RenderTreeLODFactor"), 0.1f);
	sRenderAnimateTrees = gSavedSettings.getBool("RenderAnimateTrees");
	sTreeAnimationDamping =
		llclamp(gSavedSettings.getF32("RenderTreeAnimationDamping"), 0.1f,
				1.f);
	sTreeTrunkStiffness =
		llclamp(gSavedSettings.getF32("RenderTreeTrunkStiffness"), 0.01f, 1.f);
	sTreeWindSensitivity =
		llclamp(gSavedSettings.getF32("RenderTreeWindSensitivity"), 0.00005f,
				0.05f);
}

U32 LLVOTree::processUpdateMessage(LLMessageSystem* mesgsys,
								   void** user_data, U32 block_num,
								   EObjectUpdateType update_type,
								   LLDataPacker* dp)
{
	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(mesgsys, user_data,
													  block_num, update_type,
													  dp);

	if (getVelocity().lengthSquared() > 0.f ||
		getAcceleration().lengthSquared() > 0.f ||
		getAngularVelocity().lengthSquared() > 0.f)
	{
		llwarns << "ACK !  Moving tree !" << llendl;
		setVelocity(LLVector3::zero);
		setAcceleration(LLVector3::zero);
		setAngularVelocity(LLVector3::zero);
	}

	if (update_type == OUT_TERSE_IMPROVED)
	{
		// Nothing else needs to be done for the terse message.
		return retval;
	}

	//
	// Load Instance-Specific data
	//
	if (mData)
	{
		mSpecies = ((U8*)mData)[0];
	}

	data_map_t::const_iterator it = sSpeciesTable.find(mSpecies);
	if (it == sSpeciesTable.end())
	{
		llwarns_once << "Unknown tree species: " << mSpecies
					 << ". Using default species." << llendl;
		it = sSpeciesTable.begin();
		mSpecies = it->first;
	}
	mSpeciesData = it->second;

	// Load Species-Specific data
	constexpr S32 MAX_TREE_TEXTURE_VIRTUAL_SIZE_RESET_INTERVAL = 32; // Frames.
	mTreeImagep =
		LLViewerTextureManager::getFetchedTexture(mSpeciesData->mTextureID,
												  FTT_DEFAULT, true,
												  LLGLTexture::BOOST_TERRAIN,
												  LLViewerTexture::LOD_TEXTURE);
	// Allow to wait for at most 16 frames to reset virtual size.
	mTreeImagep->setMaxVirtualSizeResetInterval(MAX_TREE_TEXTURE_VIRTUAL_SIZE_RESET_INTERVAL);

	mBranchLength = mSpeciesData->mBranchLength;
	mTrunkLength = mSpeciesData->mTrunkLength;
	mLeafScale = mSpeciesData->mLeafScale;
	mDroop = mSpeciesData->mDroop;
	mTwist = mSpeciesData->mTwist;
	mBranches = mSpeciesData->mBranches;
	mDepth = mSpeciesData->mDepth;
	mScaleStep = mSpeciesData->mScaleStep;
	mTrunkDepth = mSpeciesData->mTrunkDepth;
	mBillboardScale = mSpeciesData->mBillboardScale;
	mBillboardRatio = mSpeciesData->mBillboardRatio;
	mTrunkAspect = mSpeciesData->mTrunkAspect;
	mBranchAspect = mSpeciesData->mBranchAspect;

	// Position change not caused by us, etc.  make sure to rebuild.
	gPipeline.markRebuild(mDrawable);

	return retval;
}

void LLVOTree::idleUpdate(F64 time)
{
 	if (mDead || !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_TREE))
	{
		return;
	}

	if (sRenderAnimateTrees)
	{
		// For all tree objects, update the trunk bending with the current
		// wind. Walk sprite list in order away from viewer.
		if (!(mFrameCount % FRAMES_PER_WIND_UPDATE))
		{
			// If needed, Get latest wind for this tree
			mWind = mRegionp->mWind.getVelocity(getPositionRegion());
		}
		++mFrameCount;

		F32 mass_inv = 1.f / (5.f + mDepth * mBranches * 0.2f);
		// Pull in direction of wind
		mTrunkVel += mWind * mass_inv * sTreeWindSensitivity;
		// Restoring force in direction of trunk
		mTrunkVel -= mTrunkBend * mass_inv * sTreeTrunkStiffness;
		mTrunkBend += mTrunkVel;
		// Add damping
		mTrunkVel *= sTreeAnimationDamping;

		if (mTrunkBend.lengthSquared() > 1.f)
		{
			mTrunkBend.normalize();
		}

		if (mTrunkVel.lengthSquared() > 1.f)
		{
			mTrunkVel.normalize();
		}
	}

	U32 trunk_lod = MAX_NUM_TREE_LOD_LEVELS;
	F32 app_angle = getAppAngle() * sTreeFactor;

	for (U32 j = 0; j < MAX_NUM_TREE_LOD_LEVELS; ++j)
	{
		if (app_angle > sLODAngles[j])
		{
			trunk_lod = j;
			break;
		}
	}

	if (!sRenderAnimateTrees)
	{
		if (mReferenceBuffer.isNull() || trunk_lod != mTrunkLOD)
		{
			gPipeline.markRebuild(mDrawable);
		}
		else
		{
			// We are not animating but we may *still* need to regenerate the
			// mesh if we moved, since position and rotation are baked into the
			// mesh.
			// *TODO: I do not know what is so special about trees that they
			// do not get REBUILD_POSITION automatically at a higher level.
			const LLVector3& this_position = getPositionRegion();
			if (this_position != mLastPosition)
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_POSITION);
				mLastPosition = this_position;
			}
			else
			{
				const LLQuaternion& this_rotation = getRotation();
				if (this_rotation != mLastRotation)
				{
					gPipeline.markRebuild(mDrawable,
										  LLDrawable::REBUILD_POSITION);
					mLastRotation = this_rotation;
				}
			}
		}
	}

	mTrunkLOD = trunk_lod;
}

void LLVOTree::setPixelAreaAndAngle()
{
	LLVector3 center = getPositionAgent();	// Center of tree.
	LLVector3 viewer_pos_agent = gAgent.getCameraPositionAgent();
	LLVector3 lookAt = center - viewer_pos_agent;
	F32 dist = lookAt.normalize();
	F32 cos_angle_to_view_dir = lookAt * gViewerCamera.getXAxis();

	F32 range = dist - getMinScale() * 0.5f;
	if (range < F_ALMOST_ZERO || isHUDAttachment())		// range == zero
	{
		range = 0.f;
		mAppAngle = 180.f;
	}
	else
	{
		mAppAngle = atan2f(getMaxScale(), range) * RAD_TO_DEG;
	}

	F32 max_scale = mBillboardScale * getMaxScale();
	F32 area = mBillboardRatio * max_scale * max_scale;
	// Compute pixels per meter at the given range
	F32 pixels_per_meter = gViewerCamera.getViewHeightInPixels() /
						   (tanf(gViewerCamera.getView()) * dist);
	mPixelArea = pixels_per_meter * pixels_per_meter * area;

	F32 importance = LLFace::calcImportanceToCamera(cos_angle_to_view_dir,
													dist);
	mPixelArea = LLFace::adjustPixelArea(importance, mPixelArea);
	if (mPixelArea > gViewerCamera.getScreenPixelArea())
	{
		mAppAngle = 180.f;
	}

#if 0
	// mAppAngle is a bit of voodoo; use the one calculated with
	// LLViewerObject::setPixelAreaAndAngle above to avoid LOD
	// miscalculations
	mAppAngle = atan2f(max_scale, range) * RAD_TO_DEG;
#endif
}

void LLVOTree::updateTextures()
{
	if (mTreeImagep)
	{
		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
		{
			setDebugText(llformat("%4.0f", sqrtf(mPixelArea)));
		}
		mTreeImagep->addTextureStats(mPixelArea);
	}
}

LLDrawable* LLVOTree::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_TREE);

	LLDrawPoolTree* poolp =
		(LLDrawPoolTree*)gPipeline.getPool(LLDrawPool::POOL_TREE, mTreeImagep);

	// Just a placeholder for an actual object...
	LLFace* facep = mDrawable->addFace(poolp, mTreeImagep);
	facep->setSize(1, 3);

	updateRadius();

	return mDrawable;
}

constexpr S32 LEAF_INDICES = 24;
constexpr S32 LEAF_VERTICES = 16;

bool LLVOTree::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_TREE);

	LLFace* face = drawable->getFace(0);

	if (mTrunkLOD >= MAX_NUM_TREE_LOD_LEVELS) // Do not display the tree.
	{
		mReferenceBuffer = NULL;
		if (face)
		{
			face->setVertexBuffer(NULL);
		}
		return true;
	}

	if (mDrawable->getFace(0) &&
		(mReferenceBuffer.isNull() ||
		 !mDrawable->getFace(0)->getVertexBuffer()))
	{
		if (!face) return true;	// Abort

		constexpr F32 SRR3 = 0.577350269f; // sqrtf(1/3)
		constexpr F32 SRR2 = 0.707106781f; // sqrtf(1/2)
		U32 i, j;

		U32 slices = MAX_SLICES;

		S32 max_indices = LEAF_INDICES;
		S32 max_vertices = LEAF_VERTICES;
		U32 lod;

		face->mCenterAgent = getPositionAgent();
		face->mCenterLocal = face->mCenterAgent;

		for (lod = 0; lod < MAX_NUM_TREE_LOD_LEVELS; ++lod)
		{
			slices = sLODSlices[lod];
			sLODVertexOffset[lod] = max_vertices;
			sLODVertexCount[lod] = slices * slices;
			sLODIndexOffset[lod] = max_indices;
			sLODIndexCount[lod] = (slices - 1) * (slices - 1) * 6;
			max_indices += sLODIndexCount[lod];
			max_vertices += sLODVertexCount[lod];
		}

		mReferenceBuffer =
			new LLVertexBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
		mReferenceBuffer->setOwner("LLVOTree reference");
#endif
		if (!mReferenceBuffer->allocateBuffer(max_vertices, max_indices))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< max_vertices << " vertices and "
					<< max_indices << " indices" << llendl;
			mReferenceBuffer = NULL;
			return true;	// Abort
		}

		LLStrider<LLVector3> vertices, normals;
		LLStrider<LLVector2> tex_coords;
		LLStrider<LLColor4U> colors;
		LLStrider<U16> indicesp;
		if (!mReferenceBuffer->getVertexStrider(vertices) ||
			!mReferenceBuffer->getNormalStrider(normals) ||
			!mReferenceBuffer->getTexCoord0Strider(tex_coords) ||
			!mReferenceBuffer->getColorStrider(colors) ||
			!mReferenceBuffer->getIndexStrider(indicesp))
		{
			return false;
		}

		// First leaf
		*(normals++) =		LLVector3(-SRR2, -SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(-0.5f * LEAF_WIDTH, 0.f, 0.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR3, -SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.5f * LEAF_WIDTH, 0.f, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(-SRR3, -SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_TOP);
		*(vertices++) =		LLVector3(-0.5f * LEAF_WIDTH, 0.f, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR2, -SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.5f * LEAF_WIDTH, 0.f, 0.f);
		*(colors++) =		LLColor4U::white;

		*(indicesp++) = 0;
		*(indicesp++) = 1;
		*(indicesp++) = 2;

		*(indicesp++) = 0;
		*(indicesp++) = 3;
		*(indicesp++) = 1;

		// Same leaf, inverse winding/normals
		*(normals++) =		LLVector3(-SRR2, SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(-0.5f * LEAF_WIDTH, 0.f, 0.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR3, SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.5f * LEAF_WIDTH, 0.f, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(-SRR3, SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_TOP);
		*(vertices++) =		LLVector3(-0.5f * LEAF_WIDTH, 0.f, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR2, SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.5f * LEAF_WIDTH, 0.f, 0.f);
		*(colors++) =		LLColor4U::white;

		*(indicesp++) = 4;
		*(indicesp++) = 6;
		*(indicesp++) = 5;

		*(indicesp++) = 4;
		*(indicesp++) = 5;
		*(indicesp++) = 7;

		// next leaf
		*(normals++) =		LLVector3(SRR2, -SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.f, -0.5f * LEAF_WIDTH, 0.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR3, SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.f, 0.5f * LEAF_WIDTH, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR3, -SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.f, -0.5f * LEAF_WIDTH, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(SRR2, SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.f, 0.5f * LEAF_WIDTH, 0.f);
		*(colors++) =		LLColor4U::white;

		*(indicesp++) = 8;
		*(indicesp++) = 9;
		*(indicesp++) = 10;

		*(indicesp++) = 8;
		*(indicesp++) = 11;
		*(indicesp++) = 9;

		// Other side of same leaf
		*(normals++) =		LLVector3(-SRR2, -SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.f, -0.5f * LEAF_WIDTH, 0.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(-SRR3, SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.f, 0.5f * LEAF_WIDTH, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(-SRR3, -SRR3, SRR3);
		*(tex_coords++) =	LLVector2(LEAF_LEFT, LEAF_TOP);
		*(vertices++) =		LLVector3(0.f, -0.5f * LEAF_WIDTH, 1.f);
		*(colors++) =		LLColor4U::white;

		*(normals++) =		LLVector3(-SRR2, SRR2, 0.f);
		*(tex_coords++) =	LLVector2(LEAF_RIGHT, LEAF_BOTTOM);
		*(vertices++) =		LLVector3(0.f, 0.5f * LEAF_WIDTH, 0.f);
		*(colors++) =		LLColor4U::white;

		*(indicesp++) = 12;
		*(indicesp++) = 14;
		*(indicesp++) = 13;

		*(indicesp++) = 12;
		*(indicesp++) = 13;
		*(indicesp++) = 15;

		// Generate geometry (vertices and indices) for the cylinders for each
		// LOD.

		for (lod = 0; lod < MAX_NUM_TREE_LOD_LEVELS; ++lod)
		{
			slices = sLODSlices[lod];
			S32 offset_vertex = sLODVertexOffset[lod];
			F32 base_radius = 0.65f;
			F32 top_radius = base_radius * mSpeciesData->mTaper;
			F32 angle = 0;
			F32 angle_inc = 360.f / (F32)(slices - 1);
			F32 z = 0.f;
			F32 z_inc = 1.f;
			if (slices > 3)
			{
				z_inc = 1.f / (F32)(slices - 3);
			}
			F32 radius = base_radius;

			F32 x1, y1;
			F32 noise_scale = mSpeciesData->mNoiseMag;
			LLVector3 nvec;

			// Height to 'peak' the caps on top/bottom of branch
			constexpr F32 cap_nudge = 0.1f;
			constexpr S32 fractal_depth = 5;

			F32 nvec_scale = 1.f * mSpeciesData->mNoiseScale;
			F32 nvec_scalez = 4.f * mSpeciesData->mNoiseScale;

			F32 tex_z_repeat = mSpeciesData->mRepeatTrunkZ;

			F32 start_radius;
			F32 nangle = 0;
			F32 height = 1.f;
			F32 r0;

			for (i = 0; i < slices; ++i)
			{
				if (i == 0)
				{
					z = - cap_nudge;
					r0 = 0.f;
				}
				else if (i == slices - 1)
				{
					z = 1.f + cap_nudge; //((i - 2) * z_inc) + cap_nudge;
					r0 = 0.f;
				}
				else
				{
					z = (F32)(i - 1) * z_inc;
					r0 = base_radius + (top_radius - base_radius) * z;
				}

				for (j = 0; j < slices; ++j)
				{
					if (slices - 1 == j)
					{
						angle = 0.f;
					}
					else
					{
						angle = j * angle_inc;
					}

					nangle = angle;

					x1 = cosf(angle * DEG_TO_RAD);
					y1 = sinf(angle * DEG_TO_RAD);
					LLVector2 tc;
					// This is not totally accurate. Should compute based on
					// slope as well.
					start_radius =
						r0 * (1.f + 1.2f * fabsf(z - 0.66f * height) / height);
					nvec.set(cosf(nangle * DEG_TO_RAD) * start_radius *
							 nvec_scale,
							 sinf(nangle * DEG_TO_RAD) * start_radius *
							 nvec_scale,
							 z * nvec_scalez);
					// First and last slice at 0 radius (to bring in top/bottom
					// of structure)
					radius = start_radius +
							 turbulence3((F32*)&nvec.mV, (F32)fractal_depth) *
							 noise_scale;

					if (slices - 1 == j)
					{
						// Not 0.5 for slight slop factor to avoid edges on
						// leaves
						tc = LLVector2(0.49f, (1.f - z * 0.5f) * tex_z_repeat);
					}
					else
					{
						tc = LLVector2(angle / 720.f,
									   (1.f - z * 0.5f) * tex_z_repeat);
					}

					*vertices++ = LLVector3(x1 * radius, y1 * radius, z);
					*normals++ = LLVector3(x1, y1, 0.f);
					*tex_coords++ = tc;
					*(colors++) = LLColor4U::white;
				}
			}

			for (i = 0; i < slices - 1; ++i)
			{
				for (j = 0; j < slices - 1; ++j)
				{
					S32 x1_offset = j + 1;
					if (j + 1 == slices)
					{
						x1_offset = 0;
					}
					// Generate the matching quads
					*indicesp++ = j + i * slices + offset_vertex;
					*indicesp++ = x1_offset + (i + 1) * slices + offset_vertex;
					*indicesp++ = j + (i + 1) * slices + offset_vertex;

					*indicesp++ = j + i * slices + offset_vertex;
					*indicesp++ = x1_offset + i * slices + offset_vertex;
					*indicesp++ = x1_offset + (i + 1) * slices + offset_vertex;
				}
			}
			slices /= 2;
		}

		mReferenceBuffer->unmapBuffer();
	}

	if (sRenderAnimateTrees && mDrawable->getFace(0))
	{
		mDrawable->getFace(0)->setVertexBuffer(mReferenceBuffer);
	}
	else
	{
		// Generate tree mesh
		updateMesh();
	}

	return true;
}

void LLVOTree::updateMesh()
{
	LLMatrix4 matrix;
	// Translate to tree base  HACK - adjustment in Z plants tree underground
	const LLVector3& pos_region = getPositionRegion();
	if (pos_region.isExactlyZero())
	{
		llwarns << "Wrong region position for tree, aborting." << llendl;
	}
	LLMatrix4 trans_mat;
	trans_mat.setTranslation(pos_region.mV[VX], pos_region.mV[VY],
							 pos_region.mV[VZ] - 0.1f);
	trans_mat *= matrix;

	// Rotate to tree position and bend for current trunk/wind. Note that trunk
	// stiffness controls the amount of bend at the trunk as opposed to the
	// crown of the tree
	static const LLQuaternion qz(90.f * DEG_TO_RAD, LLVector4(0.f, 0.f, 1.f));
	F32 trunc_bend_length = mTrunkBend.length();
	LLQuaternion rot = LLQuaternion(trunc_bend_length * TRUNK_STIFF,
									LLVector4(mTrunkBend.mV[VX],
											  mTrunkBend.mV[VY], 0.f)) *
					   qz * getRotation();

	LLMatrix4 rot_mat(rot);
	rot_mat *= trans_mat;

	F32 radius = getScale().length() * 0.05f;
	LLMatrix4 scale_mat;
	scale_mat.mMatrix[0][0] = scale_mat.mMatrix[1][1] =
							  scale_mat.mMatrix[2][2] = radius;

	scale_mat *= rot_mat;

	F32 droop = mDroop + 25.f * (1.f - trunc_bend_length);

	S32 stop_depth = 0;
	F32 alpha = 1.f;

	U32 vert_count = 0;
	U32 index_count = 0;

	calcNumVerts(vert_count, index_count, mTrunkLOD, stop_depth, mDepth,
				 mTrunkDepth, mBranches);

	LLFace* facep = mDrawable->getFace(0);
	if (!facep) return;	// Abort

	if (mUpdateMeshBuffer.isNull())
	{
		mUpdateMeshBuffer =
			new LLVertexBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
		mUpdateMeshBuffer->setOwner("LLVOTree mesh");
#endif
	}
	if (!mUpdateMeshBuffer->allocateBuffer(vert_count, index_count))
	{
		llwarns << "Failure to resize a vertex buffer with " << vert_count
				<< " vertices and " << index_count << " indices" << llendl;
		mUpdateMeshBuffer->allocateBuffer(1, 3);
		mUpdateMeshBuffer->resetVertexData();
		mUpdateMeshBuffer->resetIndexData();
		facep->setSize(1, 3);
		facep->setVertexBuffer(mUpdateMeshBuffer);
		mReferenceBuffer->unmapBuffer();
		mUpdateMeshBuffer->unmapBuffer();
		return;
	}

	facep->setVertexBuffer(mUpdateMeshBuffer);

	LLStrider<LLVector3> vertices, normals;
	LLStrider<LLVector2> tex_coords;
	LLStrider<LLColor4U> colors;
	LLStrider<U16> indices;
	U16 idx_offset = 0;

	if (!mUpdateMeshBuffer->getVertexStrider(vertices) ||
		!mUpdateMeshBuffer->getNormalStrider(normals) ||
		!mUpdateMeshBuffer->getTexCoord0Strider(tex_coords) ||
		!mUpdateMeshBuffer->getColorStrider(colors) ||
		!mUpdateMeshBuffer->getIndexStrider(indices))
	{
		return;
	}

	genBranchPipeline(vertices, normals, tex_coords, colors, indices,
					  idx_offset, scale_mat, mTrunkLOD, stop_depth, mDepth,
					  mTrunkDepth, 1.f, mTwist, droop, mBranches, alpha);

	mReferenceBuffer->unmapBuffer();
	mUpdateMeshBuffer->unmapBuffer();
}

void LLVOTree::appendMesh(LLStrider<LLVector3>& vertices,
						  LLStrider<LLVector3>& normals,
						  LLStrider<LLVector2>& tex_coords,
						  LLStrider<LLColor4U>& colors,
						  LLStrider<U16>& indices, U16& cur_idx,
						  const LLMatrix4& matrix, const LLMatrix4& norm_mat,
						  S32 vert_start, S32 vert_count,
						  S32 index_count, S32 index_offset)
{
	LLStrider<LLVector3> v, n;
	LLStrider<LLVector2> t;
	LLStrider<LLColor4U> c;
	LLStrider<U16> idx;

	if (!mReferenceBuffer->getVertexStrider(v) ||
		!mReferenceBuffer->getNormalStrider(n) ||
		!mReferenceBuffer->getTexCoord0Strider(t) ||
		!mReferenceBuffer->getColorStrider(c) ||
		!mReferenceBuffer->getIndexStrider(idx))
	{
		return;
	}

	// Copy/transform vertices into mesh - check
	for (S32 i = 0; i < vert_count; ++i)
	{
		U16 index = vert_start + i;
		*vertices++ = v[index] * matrix;
		LLVector3 norm = n[index] * norm_mat;
		norm.normalize();
		*normals++ = norm;
		*tex_coords++ = t[index];
		*colors++ = c[index];
	}

	// Copy offset indices into mesh - check
	for (S32 i = 0; i < index_count; ++i)
	{
		U16 index = index_offset + i;
		*indices++ = idx[index] - vert_start + cur_idx;
	}

	// Increment index offset - check
	cur_idx += vert_count;
}

void LLVOTree::genBranchPipeline(LLStrider<LLVector3>& vertices,
								 LLStrider<LLVector3>& normals,
								 LLStrider<LLVector2>& tex_coords,
								 LLStrider<LLColor4U>& colors,
								 LLStrider<U16>& indices, U16& index_offset,
								 const LLMatrix4& matrix,
								 S32 trunk_lod, S32 stop_level,
								 U16 depth, U16 trunk_depth,
								 F32 scale, F32 twist, F32 droop,
								 F32 branches, F32 alpha)
{
	if (stop_level < 0)
	{
		return;
	}

	// Generates a tree mesh by recursing, generating branches and then a
	// 'leaf' texture.

	if (depth > stop_level)
	{
		llassert(sLODIndexCount[trunk_lod] > 0);
		F32 length = trunk_depth || scale == 1.f ? mTrunkLength
												 : mBranchLength;
		F32 aspect = trunk_depth || scale == 1.f ? mTrunkAspect
												 : mBranchAspect;

		F32 width = scale * length * aspect;
		LLMatrix4 scale_mat;
		scale_mat.mMatrix[0][0] = scale_mat.mMatrix[1][1] = width;
		scale_mat.mMatrix[2][2] = scale * length;
		scale_mat *= matrix;

		LLMatrix4a m(scale_mat);
		m.invert();
		m.transpose();
#if 0	// Do not do that: it breaks lighting of trees (can easily be seen when
		// toggling "Animate trees" on/off).
		m.invert();
#endif
		LLMatrix4 norm_mat(m.getF32ptr());

		appendMesh(vertices, normals, tex_coords, colors, indices,
				   index_offset, scale_mat, norm_mat,
				   sLODVertexOffset[trunk_lod], sLODVertexCount[trunk_lod],
				   sLODIndexCount[trunk_lod], sLODIndexOffset[trunk_lod]);

		LLMatrix4 trans_mat;
		trans_mat.setTranslation(0.f, 0.f, scale * length);
		trans_mat *= matrix;

		// Recurse to create more branches
		static const LLVector4 vec4z(0.f, 0.f, 1.f);
		static const LLQuaternion qz(20.f * DEG_TO_RAD, vec4z);
		const LLQuaternion qy(droop * DEG_TO_RAD, LLVector4(0.f, 1.f, 0.f));
		const LLQuaternion qzy(qz * qy);
		const F32 constant_twist = 360.f / branches;
		for (S32 i = 0; i < (S32)branches; ++i)
		{
			F32 angle = (constant_twist +
						 (i % 2 == 0 ? twist : -twist)) * i * DEG_TO_RAD;
			LLQuaternion qt(angle, vec4z);
			LLMatrix4 rot_mat(qzy * qt);
			rot_mat *= trans_mat;

			genBranchPipeline(vertices, normals, tex_coords, colors, indices,
							  index_offset, rot_mat, trunk_lod, stop_level,
							  depth - 1, 0, scale * mScaleStep, twist, droop,
							  branches, alpha);
		}
		// Recurse to continue trunk
		if (trunk_depth)
		{
			LLMatrix4 rot_mat(70.5f * DEG_TO_RAD, vec4z);
			rot_mat *= trans_mat; // Rotate a bit around Z when ascending
			genBranchPipeline(vertices, normals, tex_coords, colors, indices,
							  index_offset, rot_mat, trunk_lod, stop_level,
							  depth, trunk_depth - 1, scale * mScaleStep,
							  twist, droop, branches, alpha);
		}
	}
	else
	{
		// Append leaves as two 90 deg crossed quads with leaf textures
		LLMatrix4 scale_mat;
		scale_mat.mMatrix[0][0] = scale_mat.mMatrix[1][1] =
			scale_mat.mMatrix[2][2] = scale * mLeafScale;

		scale_mat *= matrix;

		LLMatrix4a m(scale_mat);
		m.invert();
		m.transpose();
		LLMatrix4 norm_mat(m.getF32ptr());

		appendMesh(vertices, normals, tex_coords, colors, indices,
				   index_offset, scale_mat, norm_mat, 0,
				   LEAF_VERTICES, LEAF_INDICES, 0);
	}
}

void LLVOTree::calcNumVerts(U32& vert_count, U32& index_count, S32 trunk_lod,
							S32 stop_level, U16 depth, U16 trunk_depth,
							F32 branches)
{
	if (stop_level >= 0)
	{
		if (depth > stop_level)
		{
			index_count += sLODIndexCount[trunk_lod];
			vert_count += sLODVertexCount[trunk_lod];

			// Recurse to create more branches
			for (S32 i = 0; i < (S32)branches; ++i)
			{
				calcNumVerts(vert_count, index_count, trunk_lod, stop_level,
							 depth - 1, 0, branches);
			}

			// Recurse to continue trunk
			if (trunk_depth)
			{
				calcNumVerts(vert_count, index_count, trunk_lod, stop_level,
							 depth, trunk_depth - 1, branches);
			}
		}
		else
		{
			index_count += LEAF_INDICES;
			vert_count += LEAF_VERTICES;
		}
	}
	else
	{
		index_count += LEAF_INDICES;
		vert_count += LEAF_VERTICES;
	}
}

U32 LLVOTree::drawBranchPipeline(LLMatrix4& matrix, U16* indicesp,
								 S32 trunk_lod, S32 stop_level, U16 depth,
								 U16 trunk_depth,  F32 scale, F32 twist,
								 F32 droop,  F32 branches, F32 alpha)
{
	U32 ret = 0;
	//
	// Draws a tree by recursing, drawing branches and then a 'leaf' texture.
	// If stop_level = -1, simply draws the whole tree as a billboarded
	// texture.
	//

	if (!LLPipeline::sReflectionRender && stop_level >= 0)
	{
		//
		// Draw the tree using recursion
		//
		if (depth > stop_level)
		{
			F32 length = trunk_depth || scale == 1.f ? mTrunkLength
													 : mBranchLength;
			F32 aspect = trunk_depth || scale == 1.f ? mTrunkAspect
													 : mBranchAspect;
			{
				llassert(sLODIndexCount[trunk_lod] > 0);

				F32 width = scale * length * aspect;
				LLMatrix4 scale_mat;
				scale_mat.mMatrix[0][0] = width;
				scale_mat.mMatrix[1][1] = width;
				scale_mat.mMatrix[2][2] = scale * length;
				scale_mat *= matrix;

				gGL.loadMatrix(scale_mat.getF32ptr());
				gGL.syncMatrices();
 				glDrawElements(GL_TRIANGLES, sLODIndexCount[trunk_lod],
							   GL_UNSIGNED_SHORT,
							   indicesp + sLODIndexOffset[trunk_lod]);
				gPipeline.addTrianglesDrawn(LEAF_INDICES);
				ret += sLODIndexCount[trunk_lod];
			}

			LLMatrix4 trans_mat;
			trans_mat.setTranslation(0.f, 0.f, scale * length);
			trans_mat *= matrix;

			// Recurse to create more branches
			static const LLVector4 vec4z(0.f, 0.f, 1.f);
			static const LLQuaternion qz(20.f * DEG_TO_RAD, vec4z);
			const LLQuaternion qy(droop * DEG_TO_RAD, LLVector4(0.f, 1.f, 0.f));
			const LLQuaternion qzy(qz * qy);
			const F32 constant_twist = 360.f / branches;
			for (S32 i = 0; i < (S32)branches; ++i)
			{
				F32 angle = (constant_twist +
							 (i % 2 == 0 ? twist : -twist)) * i * DEG_TO_RAD;
				LLQuaternion qt(angle, vec4z);
				LLMatrix4 rot_mat(qzy * qt);
				rot_mat *= trans_mat;

				ret += drawBranchPipeline(rot_mat, indicesp, trunk_lod,
										  stop_level, depth - 1, 0,
										  scale * mScaleStep, twist, droop,
										  branches, alpha);
			}
			// Recurse to continue trunk
			if (trunk_depth)
			{
				LLMatrix4 rot_mat(70.5f * DEG_TO_RAD, vec4z);
				rot_mat *= trans_mat;	// Rotate a bit around Z when ascending
				ret += drawBranchPipeline(rot_mat, indicesp, trunk_lod,
										  stop_level, depth, trunk_depth - 1,
										  scale * mScaleStep, twist, droop,
										  branches, alpha);
			}
		}
		else
		{
			//
			// Draw leaves as two 90 deg crossed quads with leaf textures
			//
			LLMatrix4 scale_mat;
			scale_mat.mMatrix[0][0] =
				scale_mat.mMatrix[1][1] =
				scale_mat.mMatrix[2][2] = scale * mLeafScale;

			scale_mat *= matrix;

			gGL.loadMatrix(scale_mat.getF32ptr());
			gGL.syncMatrices();
			glDrawElements(GL_TRIANGLES, LEAF_INDICES, GL_UNSIGNED_SHORT,
						   indicesp);
			gPipeline.addTrianglesDrawn(LEAF_INDICES);
			ret += LEAF_INDICES;
		}
	}
	else
	{
		//
		// Draw the tree as a single billboard texture
		//
		LLMatrix4 scale_mat;
		scale_mat.mMatrix[0][0] =
			scale_mat.mMatrix[1][1] =
			scale_mat.mMatrix[2][2] = mBillboardScale * mBillboardRatio;

		scale_mat *= matrix;

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.translatef(0.f, -0.5f, 0.f);
		gGL.matrixMode(LLRender::MM_MODELVIEW);

		gGL.loadMatrix(scale_mat.getF32ptr());
		gGL.syncMatrices();
		glDrawElements(GL_TRIANGLES, LEAF_INDICES, GL_UNSIGNED_SHORT,
					   indicesp);
		gPipeline.addTrianglesDrawn(LEAF_INDICES);
		ret += LEAF_INDICES;

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}

	stop_glerror();

	return ret;
}

void LLVOTree::updateRadius()
{
	if (mDrawable.notNull())
	{
		mDrawable->setRadius(32.0f);
	}
}

void LLVOTree::updateSpatialExtents(LLVector4a& new_min, LLVector4a& new_max)
{
	F32 radius = getScale().length() * 0.05f;
	LLVector3 center = getRenderPosition();

	F32 sz = mBillboardScale * mBillboardRatio * radius * 0.5f;
	LLVector3 size(sz, sz, sz);

	center += LLVector3(0.f, 0.f, size.mV[2]) * getRotation();

	new_min.load3((center - size).mV);
	new_max.load3((center + size).mV);

	LLVector4a pos;
	pos.load3(center.mV);
	mDrawable->setPositionGroup(pos);
}

bool LLVOTree::lineSegmentIntersect(const LLVector4a& start,
									const LLVector4a& end,
									S32 face,
									bool pick_transparent,
									bool pick_rigged,
									S32* face_hitp,
									LLVector4a* intersection,
									LLVector2* tex_coord,
									LLVector4a* normal,
									LLVector4a* tangent)
{
	if (!lineSegmentBoundingBox(start, end))
	{
		return false;
	}

	const LLVector4a* exta = mDrawable->getSpatialExtents();

	// VECTORIZE THIS
	LLVector3 ext[2];
	ext[0].set(exta[0].getF32ptr());
	ext[1].set(exta[1].getF32ptr());

	LLVector3 center = (ext[1] + ext[0]) * 0.5f;
	LLVector3 size = ext[1] - ext[0];

	LLQuaternion quat = getRotation();

	center -= LLVector3(0.f, 0.f, size.length() * 0.25f) * quat;

	size.scaleVec(LLVector3(0.25f, 0.25f, 1.f));
	size.mV[0] = llmin(size.mV[0], 1.f);
	size.mV[1] = llmin(size.mV[1], 1.f);

	LLVector3 pos, norm;

	LLVector3 start3(start.getF32ptr());
	LLVector3 end3(end.getF32ptr());

	if (linesegment_tetrahedron(start3, end3, center, size, quat, pos, norm))
	{
		if (intersection)
		{
			intersection->load3(pos.mV);
		}

		if (normal)
		{
			normal->load3(norm.mV);
		}

		return true;
	}

	return false;
}

U32 LLVOTree::getPartitionType() const
{
	return LLViewerRegion::PARTITION_TREE;
}

void LLVOTree::generateSilhouetteVertices(std::vector<LLVector3>& vertices,
										  std::vector<LLVector3>& normals,
										  const LLVector3& obj_cam_vec,
										  const LLMatrix4& local_matrix,
										  const LLMatrix3& normal_matrix)
{
	vertices.clear();
	normals.clear();

	F32 height = mBillboardScale; // *mBillboardRatio * 0.5;
	F32 width = height * mTrunkAspect;

	LLVector3 position1 = LLVector3(-width * 0.5f, 0.f, 0.f) * local_matrix;
	LLVector3 position2 = LLVector3(-width * 0.5f, 0.f, height) * local_matrix;
	LLVector3 position3 = LLVector3(width * 0.5f, 0.f, height) * local_matrix;
	LLVector3 position4 = LLVector3(width * 0.5f, 0.f, 0.f) * local_matrix;

	LLVector3 position5 = LLVector3(0.f, -width * 0.5f, 0.f) * local_matrix;
	LLVector3 position6 = LLVector3(0.f, -width * 0.5f, height) * local_matrix;
	LLVector3 position7 = LLVector3(0.f, width * 0.5f, height) * local_matrix;
	LLVector3 position8 = LLVector3(0.f, width * 0.5f, 0.f) * local_matrix;

	LLVector3 normal = (position1 - position2) % (position2 - position3);
	normal.normalize();

	vertices.emplace_back(position1);
	normals.emplace_back(normal);
	vertices.emplace_back(position2);
	normals.emplace_back(normal);

	vertices.emplace_back(position2);
	normals.emplace_back(normal);
	vertices.emplace_back(position3);
	normals.emplace_back(normal);

	vertices.emplace_back(position3);
	normals.emplace_back(normal);
	vertices.emplace_back(position4);
	normals.emplace_back(normal);

	vertices.emplace_back(position4);
	normals.emplace_back(normal);
	vertices.emplace_back(position1);
	normals.emplace_back(normal);

	normal = (position5 - position6) % (position6 - position7);
	normal.normalize();

	vertices.emplace_back(position5);
	normals.emplace_back(normal);
	vertices.emplace_back(position6);
	normals.emplace_back(normal);

	vertices.emplace_back(position6);
	normals.emplace_back(normal);
	vertices.emplace_back(position7);
	normals.emplace_back(normal);

	vertices.emplace_back(position7);
	normals.emplace_back(normal);
	vertices.emplace_back(position8);
	normals.emplace_back(normal);

	vertices.emplace_back(position8);
	normals.emplace_back(normal);
	vertices.emplace_back(position5);
	normals.emplace_back(normal);
}

void LLVOTree::generateSilhouette(LLSelectNode* nodep)
{
	LLVector3 position;
	LLQuaternion rotation;
	if (!mDrawable->isActive())
	{
		position = getPosition() + getRegion()->getOriginAgent();
		rotation = getRotation();
	}
	else if (!mDrawable->isSpatialRoot())
	{
		position = mDrawable->getPosition();
		rotation = mDrawable->getRotation();
	}

	// Trees have strange scaling rules...
	F32 radius = getScale().length() * 0.05f;
	// Compose final matrix
	LLMatrix4 local_matrix;
	local_matrix.initAll(LLVector3(radius, radius, radius), rotation,
						 position);

	generateSilhouetteVertices(nodep->mSilhouetteVertices,
							   nodep->mSilhouetteNormals, LLVector3::zero,
							   local_matrix, LLMatrix3());

	nodep->mSilhouetteGenerated = true;
}
