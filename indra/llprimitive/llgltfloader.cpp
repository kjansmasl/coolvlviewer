/**
 * @file llgltfloader.cpp
 * @brief LLGLTFLoader implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

// Import & define single-header gltf import/export lib
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14  // default is C++11
// To avoid an issue with missing <filesystem> header with gcc 7. HB
#define JSON_HAS_CPP_14
// tinygltf by default loads image files using STB
#define STB_IMAGE_IMPLEMENTATION
// To use our own image loading:
// 1.- replace this definition with TINYGLTF_NO_STB_IMAGE
// 2.- provide image loader callback with
// TinyGLTF::SetImageLoader(LoadimageDataFunction LoadImageData, void* data)
// tinygltf saves image files using STB
#define STB_IMAGE_WRITE_IMPLEMENTATION
// similarly, can override with TINYGLTF_NO_STB_IMAGE_WRITE and
// TinyGLTF::SetImageWriter(fxn, data)
// Additionally, disable inclusion of STB header files entirely with
// TINYGLTF_NO_INCLUDE_STB_IMAGE
// TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "tinygltf/tiny_gltf.h"

// For GL_LINEAR
#include "epoxy/gl.h"

#include "llgltfloader.h"

static const std::string lod_suffix[LLModel::NUM_LODS] =
{
	"_LOD0",
	"_LOD1",
	"_LOD2",
	"",
	"_PHYS",
};

LLGLTFLoader::LLGLTFLoader(const std::string& filename, S32 lod,
						   load_callback_t load_cb,
						   joint_lookup_func_t joint_lookup_func,
						   texture_load_func_t texture_load_func,
						   state_callback_t state_cb, void* userdata,
						   JointTransformMap& joint_transform_map,
						   JointNameSet& joints_from_nodes,
						   JointMap& legal_joint_names,
						   U32 max_joints_per_mesh)
:	LLModelLoader(filename, lod, load_cb, joint_lookup_func, texture_load_func,
				  state_cb, userdata, joint_transform_map, joints_from_nodes,
				  legal_joint_names, max_joints_per_mesh),
	mMeshesLoaded(false),
	mMaterialsLoaded(false)
{
}

bool LLGLTFLoader::openFile(const std::string& filename)
{
	tinygltf::TinyGLTF loader;
	std::string error_msg, warn_msg;
	std::string filename_lc(filename);
	LLStringUtil::toLower(filename_lc);

	// Load a tinygltf model fom a file. Assumes that the input filename has
	// already been sanitized to one of (.gltf , .glb) extensions, so does a
	// simple find to distinguish.
	if (std::string::npos == filename_lc.rfind(".gltf"))
	{
		// File is binary
		mGltfLoaded = loader.LoadBinaryFromFile(&mGltfModel, &error_msg,
												&warn_msg, filename);
	}
	else
	{
		// File is ASCII
		mGltfLoaded = loader.LoadASCIIFromFile(&mGltfModel, &error_msg,
											   &warn_msg, filename);
	}

	if (!mGltfLoaded)
	{
		if (!warn_msg.empty())
		{
			llwarns << "gltf load warning: " << warn_msg.c_str() << llendl;
		}
		if (!error_msg.empty())
		{
			llwarns << "gltf load error: " << error_msg.c_str() << llendl;
		}
		return false;
	}

	mMeshesLoaded = parseMeshes();
#if 0	// *TOTO
	if (mMeshesLoaded)
	{
		uploadMeshes();
	}
#endif

	mMaterialsLoaded = parseMaterials();
	if (mMaterialsLoaded)
	{
		uploadMaterials();
	}

	return mMeshesLoaded || mMaterialsLoaded;
}

bool LLGLTFLoader::parseMeshes()
{
	if (!mGltfLoaded) return false;

	// 2022-04 DJH Volume params from dae example. *TODO understand PCODE
	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);

	for (tinygltf::Mesh mesh : mGltfModel.meshes)
	{
		LLModel* modelp = new LLModel(volume_params, 0.f);

		if (populateModelFromMesh(modelp, mesh) &&
			modelp->getStatus() == LLModel::NO_ERRORS &&
			modelp->validate(true))
		{
			mModelList.push_back(modelp);
		}
		else
		{
			setLoadState(ERROR_MODEL + modelp->getStatus());
			delete modelp;
			return false;
		}
	}
	return true;
}

bool LLGLTFLoader::populateModelFromMesh(LLModel* modelp,
										 const tinygltf::Mesh& mesh)
{
	modelp->mLabel = mesh.name;
	tinygltf::Accessor indices_a, positions_a, normals_a, uv0_a, color0_a;
	auto prims = mesh.primitives;
	for (auto prim : prims)
	{
		if (prim.indices >= 0)
		{
			indices_a = mGltfModel.accessors[prim.indices];
		}

		S32 pos_idx = -1;
		if (prim.attributes.count("POSITION"))
		{
			pos_idx =  prim.attributes.at("POSITION");
		}
		if (pos_idx >= 0)
		{
			positions_a = mGltfModel.accessors[pos_idx];
			if (positions_a.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				continue;
			}
			auto positions_bv = mGltfModel.bufferViews[positions_a.bufferView];
			auto positions_buf = mGltfModel.buffers[positions_bv.buffer];
		}

#if 0
		S32 norm_idx = prim.attributes.count("NORMAL") ? prim.attributes.at("NORMAL") : -1;
		S32 tan_idx = prim.attributes.count("TANGENT") ? prim.attributes.at("TANGENT") : -1;
		S32 uv0_idx = prim.attributes.count("TEXCOORDS_0") ? prim.attributes.at("TEXCOORDS_0") : -1;
		S32 uv1_idx = prim.attributes.count("TEXCOORDS_1") ? prim.attributes.at("TEXCOORDS_1") : -1;
		S32 color0_idx = prim.attributes.count("COLOR_0") ? prim.attributes.at("COLOR_0") : -1;
		S32 color1_idx = prim.attributes.count("COLOR_1") ? prim.attributes.at("COLOR_1") : -1;

		if (prim.mode == TINYGLTF_MODE_TRIANGLES)
		{
			// auto pos = mesh.	TODO resume here DJH 2022-04
		}
#endif
	}

#if 0
	modelp->addFace()
#endif
	return false;
}

bool LLGLTFLoader::parseMaterials()
{
	if (!mGltfLoaded) return false;

	// Fill local texture data structures
	mSamplers.clear();
	for (auto in_sampler : mGltfModel.samplers)
	{
		gltf_sampler sampler;
		sampler.magFilter =
			in_sampler.magFilter > 0 ? in_sampler.magFilter : GL_LINEAR;
		sampler.minFilter =
			in_sampler.minFilter > 0 ? in_sampler.minFilter : GL_LINEAR;
		sampler.wrapS = in_sampler.wrapS;
		sampler.wrapT = in_sampler.wrapT;
		sampler.name = in_sampler.name; // unused
		mSamplers.push_back(sampler);
	}

	mImages.clear();
	for (auto in_image : mGltfModel.images)
	{
		gltf_image image;
		image.numChannels = in_image.component;
		image.bytesPerChannel = in_image.bits >> 3;	 // Convert bits to bytes
		// Maps exactly, i.e.
		// TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE == GL_UNSIGNED_BYTE, etc
		image.pixelType = in_image.pixel_type;
		image.size = in_image.image.size();
		image.height = in_image.height;
		image.width = in_image.width;
		image.data = in_image.image.data();

		if (in_image.as_is)
		{
			llwarns << "Unsupported image encoding" << llendl;
			return false;
		}

		if (image.size != image.height * image.width * image.numChannels *
						  image.bytesPerChannel)
		{
			llwarns << "Image size error" << llendl;
			return false;
		}

		mImages.push_back(image);
	}

	mTextures.clear();
	for (auto in_tex : mGltfModel.textures)
	{
		gltf_texture tex;
		tex.imageIdx = in_tex.source;
		tex.samplerIdx = in_tex.sampler;
		tex.imageUuid.setNull();

		if (tex.imageIdx >= mImages.size() || tex.samplerIdx >= mSamplers.size())
		{
			llwarns << "Texture sampler/image index error" << llendl;
			return false;
		}

		mTextures.push_back(tex);
	}

	// Parse each material
	for (tinygltf::Material gltf_material : mGltfModel.materials)
	{
		gltf_render_material mat;
		mat.name = gltf_material.name;

		tinygltf::PbrMetallicRoughness& pbr = gltf_material.pbrMetallicRoughness;
		mat.hasPBR = true;  // Always true, for now

		mat.baseColor.set(pbr.baseColorFactor.data());
		mat.hasBaseTex = pbr.baseColorTexture.index >= 0;
		mat.baseColorTexIdx = pbr.baseColorTexture.index;
		mat.baseColorTexCoords = pbr.baseColorTexture.texCoord;

		mat.metalness = pbr.metallicFactor;
		mat.roughness = pbr.roughnessFactor;
		mat.hasMRTex = pbr.metallicRoughnessTexture.index >= 0;
		mat.metalRoughTexIdx = pbr.metallicRoughnessTexture.index;
		mat.metalRoughTexCoords = pbr.metallicRoughnessTexture.texCoord;

		mat.normalScale = gltf_material.normalTexture.scale;
		mat.hasNormalTex = gltf_material.normalTexture.index >= 0;
		mat.normalTexIdx = gltf_material.normalTexture.index;
		mat.normalTexCoords = gltf_material.normalTexture.texCoord;

		mat.occlusionScale = gltf_material.occlusionTexture.strength;
		mat.hasOcclusionTex = gltf_material.occlusionTexture.index >= 0;
		mat.occlusionTexIdx = gltf_material.occlusionTexture.index;
		mat.occlusionTexCoords = gltf_material.occlusionTexture.texCoord;

		mat.emissiveColor.set(gltf_material.emissiveFactor.data());
		mat.hasEmissiveTex = gltf_material.emissiveTexture.index >= 0;
		mat.emissiveTexIdx = gltf_material.emissiveTexture.index;
		mat.emissiveTexCoords = gltf_material.emissiveTexture.texCoord;

		mat.alphaMode = gltf_material.alphaMode;
		mat.alphaMask = gltf_material.alphaCutoff;

		size_t tex_size = mTextures.size();
		if ((mat.hasNormalTex && mat.normalTexIdx >= tex_size) ||
			(mat.hasOcclusionTex && mat.occlusionTexIdx >= tex_size) ||
			(mat.hasEmissiveTex && mat.emissiveTexIdx >= tex_size) ||
			(mat.hasBaseTex && mat.baseColorTexIdx  >= tex_size) ||
			(mat.hasMRTex && mat.metalRoughTexIdx >= tex_size))
		{
			llwarns << "Texture resource index error" << llendl;
			return false;
		}

		// Note: mesh can have up to 3 sets of UV
		if ((mat.hasNormalTex && mat.normalTexCoords > 2) ||
			(mat.hasOcclusionTex && mat.occlusionTexCoords > 2) ||
			(mat.hasEmissiveTex && mat.emissiveTexCoords > 2) ||
			(mat.hasBaseTex && mat.baseColorTexCoords > 2) ||
			(mat.hasMRTex && mat.metalRoughTexCoords > 2))
		{
			llwarns << "Image texcoord index error" << llendl;
			return false;
		}

		mMaterials.emplace_back(mat);
	}

	return true;
}

// Convert raw image buffers to texture UUIDs & assemble into a render material
void LLGLTFLoader::uploadMaterials()
{
	// Initially 1 material per gltf file, but design for multiple
	for (gltf_render_material mat : mMaterials)
	{
		if (mat.hasBaseTex)
		{
			gltf_texture& gtex = mTextures[mat.baseColorTexIdx];
			if (gtex.imageUuid.isNull())
			{
				gtex.imageUuid = imageBufferToTextureUUID(gtex);
			}
		}

		if (mat.hasMRTex)
		{
			gltf_texture& gtex = mTextures[mat.metalRoughTexIdx];
			if (gtex.imageUuid.isNull())
			{
				gtex.imageUuid = imageBufferToTextureUUID(gtex);
			}
		}

		if (mat.hasNormalTex)
		{
			gltf_texture& gtex = mTextures[mat.normalTexIdx];
			if (gtex.imageUuid.isNull())
			{
				gtex.imageUuid = imageBufferToTextureUUID(gtex);
			}
		}

		if (mat.hasOcclusionTex)
		{
			gltf_texture& gtex = mTextures[mat.occlusionTexIdx];
			if (gtex.imageUuid.isNull())
			{
				gtex.imageUuid = imageBufferToTextureUUID(gtex);
			}
		}

		if (mat.hasEmissiveTex)
		{
			gltf_texture& gtex = mTextures[mat.emissiveTexIdx];
			if (gtex.imageUuid.isNull())
			{
				gtex.imageUuid = imageBufferToTextureUUID(gtex);
			}
		}
	}
}
