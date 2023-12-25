/**
 * @file llgltfloader.h
 * @brief LLGLTFLoader definition
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

#ifndef LL_LLGLTFLOADER_H
#define LL_LLGLTFLOADER_H

#include "llmodelloader.h"

namespace tinygltf
{
	class Model;
	struct Mesh;
}

// gltf_* structs are temporary, used to organize the subset of data that
// eventually goes into the material LLSD.

// Uses GL enums
class gltf_sampler
{
public:
	// GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST,
	// GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR or
	// GL_LINEAR_MIPMAP_LINEAR
	S32			minFilter;
	S32			magFilter;	// GL_NEAREST or GL_LINEAR
	S32			wrapS;		// GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, GL_REPEAT
	S32			wrapT;		// GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, GL_REPEAT
#if 0	// Found in some sample files, but not part of glTF 2.0 spec. Ignored.
	S32			wrapR;
#endif
	std::string	name;		// Optional, currently unused
	// Extensions and extras are sampler optional fields that we do not
	// support, at least initially.
};

class gltf_image
{
public:
	// Note that glTF images are defined with row 0 at the top (opposite of
	// OpenGL).
	U8* data;			// Pointer to decoded image data
	U32 size;			// In bytes, regardless of channel width
	U32 width;
	U32 height;
	U32 numChannels;	// Range 1..4
	// Converted from gltf "bits", expects only 8, 16 or 32 as input
	U32 bytesPerChannel;
	// One of (TINYGLTF_COMPONENT_TYPE)_UNSIGNED_BYTE, _UNSIGNED_SHORT,
	// _UNSIGNED_INT, or _FLOAT
	U32 pixelType;
};

class gltf_texture
{
public:
	LLUUID	imageUuid;
	U32		imageIdx;
	U32		samplerIdx;
};

class gltf_render_material
{
public:
	std::string	name;

	// This field is populated after upload
	LLUUID		material_uuid;

	// *TODO: add traditional (diffuse, normal, specular) UUIDs here, or add
	// this struct to LLTextureEntry ?

	// Scalar values

	// Linear encoding. Multiplied with vertex color, if present.
	LLColor4	baseColor;
	// Emissive mulitiplier, assumed linear encoding (spec 2.0 is silent)
	LLColor4	emissiveColor;
	std::string	alphaMode;		// "OPAQUE", "MASK" or "BLEND"
	double		alphaMask;		// Alpha cut-off
	double		metalness;
	double		roughness;
	// Scale applies only to X,Y components of normal
	double		normalScale;
	double		occlusionScale;	// Strength multiplier for occlusion

	// Textures

	// Always sRGB encoded
	U32			baseColorTexIdx;
	// Always linear, roughness in G channel, metalness in B channel
	U32			metalRoughTexIdx;
	// Linear, valid range R[0-1], G[0-1], B[0.5-1].
	// Normal = texel * 2 - vec3(1.0)
	U32			normalTexIdx;
	// Linear, occlusion in R channel, 0 meaning fully occluded, 1 meaning not
	// occluded.
	U32			occlusionTexIdx;
	 // Always stored as sRGB, in nits (candela / meter^2)
	U32			emissiveTexIdx;

	// Texture coordinates

	U32			baseColorTexCoords;
	U32			metalRoughTexCoords;
	U32			normalTexCoords;
	U32			occlusionTexCoords;
	U32			emissiveTexCoords;

	bool		hasPBR;
	bool		hasBaseTex;
	bool		hasMRTex;
	bool		hasNormalTex;
	bool		hasOcclusionTex;
	bool		hasEmissiveTex;
};

class LLGLTFLoader : public LLModelLoader
{
protected:
	LOG_CLASS(LLGLTFLoader);

public:
	typedef std::map<std::string, LLImportMaterial> material_map;

	LLGLTFLoader(const std::string& filename, S32 lod,
				 load_callback_t load_cb,
				 joint_lookup_func_t joint_lookup_func,
				 texture_load_func_t texture_load_func,
				 state_callback_t state_cb, void* userdata,
				 JointTransformMap& joint_transform_map,
				 JointNameSet& joints_from_nodes,
				 JointMap& legal_joint_names, U32 max_joints_per_mesh);

	virtual bool openFile(const std::string& filename);

private:
	bool parseMeshes();
#if 0	// *TOTO
	void uploadMeshes();
#endif
	bool parseMaterials();
	void uploadMaterials();
	bool populateModelFromMesh(LLModel* modelp, const tinygltf::Mesh& mesh);
	// *TOTO
	LLUUID imageBufferToTextureUUID(const gltf_texture& tex)
	{
		return LLUUID::null;
	}

protected:
	tinygltf::Model						mGltfModel;
	std::vector<gltf_render_material>	mMaterials;
	std::vector<gltf_texture>			mTextures;
	std::vector<gltf_image>				mImages;
	std::vector<gltf_sampler>			mSamplers;
	bool								mGltfLoaded;
	bool								mMeshesLoaded;
	bool								mMaterialsLoaded;
};

#endif  //  LL_LLGLTFLOADER_H
