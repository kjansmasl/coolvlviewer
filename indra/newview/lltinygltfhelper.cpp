/**
 * @file lltinygltfhelper.h
 * @brief The LLTinyGLTFHelper class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lltinygltfhelper.h"

#include "lldir.h"
#include "llimage.h"

#include "llviewertexture.h"
#include "llviewertexturelist.h"

static void strip_alpha_channel(LLPointer<LLImageRaw>& img)
{
	if (img->getComponents() == 4)
	{
		LLImageRaw* tmp = new LLImageRaw(img->getWidth(), img->getHeight(), 3);
		tmp->copyUnscaled4onto3(img);
		img = tmp;
	}
}

// Copy red channel from src_img to dst_img. Preconditions: dst_img must have 3
// components, src_img and dst_image must have the same dimensions.
static void copy_red_channel(LLPointer<LLImageRaw>& src_img,
							 LLPointer<LLImageRaw>& dst_img)
{
	llassert(src_img->getWidth() == dst_img->getWidth() &&
			 src_img->getHeight() == dst_img->getHeight() &&
			 dst_img->getComponents() == 3);

	U32 pixel_count = dst_img->getWidth() * dst_img->getHeight();
	U8* src = src_img->getData();
	U8* dst = dst_img->getData();
	S8 src_components = src_img->getComponents();

	for (U32 i = 0; i < pixel_count; ++i)
	{
		dst[i * 3] = src[i * src_components];
	}
}

//static
void LLTinyGLTFHelper::initFetchedTextures(tinygltf::Material& material,
										   LLPointer<LLImageRaw>& basecol_imgp,
										   LLPointer<LLImageRaw>& normal_imgp,
                                           LLPointer<LLImageRaw>& mr_imgp,
										   LLPointer<LLImageRaw>& emissive_imgp,
										   LLPointer<LLImageRaw>& occl_imgp,
                                           LLPointer<LLViewerFetchedTexture>& basecolp,
										   LLPointer<LLViewerFetchedTexture>& normalp,
										   LLPointer<LLViewerFetchedTexture>& mrp,
										   LLPointer<LLViewerFetchedTexture>& emissivep)
{
	if (basecol_imgp)
	{
		basecolp = LLViewerTextureManager::getFetchedTexture(basecol_imgp);
	}

	if (normal_imgp)
	{
		strip_alpha_channel(normal_imgp);
		normalp = LLViewerTextureManager::getFetchedTexture(normal_imgp);
	}

	if (mr_imgp)
	{
		strip_alpha_channel(mr_imgp);
		if (occl_imgp &&
			material.pbrMetallicRoughness.metallicRoughnessTexture.index !=
				material.occlusionTexture.index)
		{
			// Occlusion is a distinct texture from pbrMetallicRoughness; pack
			// into mr red channel.
			S32 occlusion_idx = material.occlusionTexture.index;
			S32 mr_idx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
			if (occlusion_idx != mr_idx)
			{
				// Scale occlusion image to match resolution of mr image
				occl_imgp->scale(mr_imgp->getWidth(), mr_imgp->getHeight());
				copy_red_channel(occl_imgp, mr_imgp);
			}
		}
		mrp = LLViewerTextureManager::getFetchedTexture(mr_imgp);
	}
	else if (occl_imgp)
	{
		// No mr but occlusion exists, make a white mr_imgp and copy occlusion
		// red channel over.
		mr_imgp = new LLImageRaw(occl_imgp->getWidth(), occl_imgp->getHeight(), 3);
		mr_imgp->clear(255, 255, 255);
		copy_red_channel(occl_imgp, mr_imgp);
	}

	if (emissive_imgp)
	{
		strip_alpha_channel(emissive_imgp);
		emissivep = LLViewerTextureManager::getFetchedTexture(emissive_imgp);
	}
}

//static
LLColor4 LLTinyGLTFHelper::getColor(const std::vector<double>& in)
{
	LLColor4 out;
	for (S32 i = 0, count = llmin((S32)in.size(), 4); i < count; ++i)
	{
		out.mV[i] = in[i];
	}
	return out;
}

//static
const tinygltf::Image* LLTinyGLTFHelper::getImageFromTextureIndex(const tinygltf::Model& m,
																  S32 tex_idx)
{
	if (tex_idx >= 0)
	{
		S32 src_idx = m.textures[tex_idx].source;
		if (src_idx >= 0)
		{
			return &(m.images[src_idx]);
		}
	}
	return NULL;
}

//static
LLImageRaw* LLTinyGLTFHelper::getTexture(const std::string& folder,
										 const tinygltf::Model& model,
										 S32 tex_idx, std::string& name)
{
	const tinygltf::Image* imgp = getImageFromTextureIndex(model, tex_idx);
	LLImageRaw* rawp = NULL;
	if (imgp && imgp->bits == 8 && !imgp->image.empty() &&
		imgp->component <= 4)
	{
		name = imgp->name;
		rawp = new LLImageRaw(&imgp->image[0], imgp->width, imgp->height,
							  imgp->component);
		rawp->verticalFlip();
		rawp->optimizeAwayAlpha();
	}
	return rawp;
}

//static
LLImageRaw* LLTinyGLTFHelper::getTexture(const std::string& folder,
										 const tinygltf::Model& model,
										 S32 tex_idx)
{
	const tinygltf::Image* imgp = getImageFromTextureIndex(model, tex_idx);
	LLImageRaw* rawp = NULL;
	if (imgp && imgp->bits == 8 && !imgp->image.empty() &&
		imgp->component <= 4)
	{
		rawp = new LLImageRaw(&imgp->image[0], imgp->width, imgp->height,
							  imgp->component);
		rawp->verticalFlip();
		rawp->optimizeAwayAlpha();
	}
	return rawp;
}

//static
bool LLTinyGLTFHelper::loadModel(const std::string& filename,
								 tinygltf::Model& model_in)
{
	std::string exten = gDirUtilp->getExtension(filename);
	LLStringUtil::toLower(exten);
	if (exten != "gltf" && exten != "glb")
	{
		llwarns << "Invalid extension for a GLTF model file: " << filename
				<< llendl;
		return false;
	}

	tinygltf::TinyGLTF loader;
	std::string error_msg, warn_msg;
	bool success;
	if (exten == "gltf")
	{
		// File is ASCII
		success = loader.LoadASCIIFromFile(&model_in, &error_msg, &warn_msg,
										   filename);
	}
	else
	{
		// File is binary
		success = loader.LoadBinaryFromFile(&model_in, &error_msg, &warn_msg,
											filename);
	}
	if (!success)
	{
		llwarns << "Failed to load file: " << filename << " - Error: "
				<< error_msg << " - Warning: " << warn_msg << llendl;
		return false;
	}

	if (model_in.materials.empty())
	{
		// Materials are missing
		llwarns << "Load failed. No material found in file: " << filename
				<< llendl;
		return false;
	}

	return true;
}

//static
bool LLTinyGLTFHelper::getMaterialFromModel(const std::string& filename,
											const tinygltf::Model& model_in,
											S32 mat_idx,
											LLFetchedGLTFMaterial* matp,
											std::string& mat_name)
{
	if (!matp)
	{
		llwarns << "NULL material passed for " << filename << llendl;
		llassert(false);
		return false;
	}

	if (!matp || (S32)model_in.materials.size() <= mat_idx)
	{
		// Materials are missing
		llwarns << "Cannot load. No material at index " << mat_idx
				<< " in file " << filename << llendl;
		return false;
	}

	matp->setFromModel(model_in, mat_idx);

	std::string folder = gDirUtilp->getDirName(filename);

	tinygltf::Material mat_in = model_in.materials[mat_idx];
	mat_name = mat_in.name;

	// Get base color texture
	LLPointer<LLImageRaw> base_img =
		getTexture(folder, model_in,
				   mat_in.pbrMetallicRoughness.baseColorTexture.index);
	// Get normal map
	LLPointer<LLImageRaw> normal_imgp = getTexture(folder, model_in,
												   mat_in.normalTexture.index);
	// Get metallic-roughness texture
	LLPointer<LLImageRaw> mr_imgp =
		getTexture(folder, model_in,
				   mat_in.pbrMetallicRoughness.metallicRoughnessTexture.index);
	// Get emissive texture
	LLPointer<LLImageRaw> emissive_imgp =
		getTexture(folder, model_in, mat_in.emissiveTexture.index);
	// Get occlusion map if needed
	LLPointer<LLImageRaw> occl_imgp;
	if (mat_in.occlusionTexture.index !=
			mat_in.pbrMetallicRoughness.metallicRoughnessTexture.index)
	{
		occl_imgp = getTexture(folder, model_in,
							   mat_in.occlusionTexture.index);
	}

	LLPointer<LLViewerFetchedTexture> basecolp;
	LLPointer<LLViewerFetchedTexture> normalp;
	LLPointer<LLViewerFetchedTexture> mrp;
	LLPointer<LLViewerFetchedTexture> emissivep;
	// *TODO: pass it into local bitmaps ?
	initFetchedTextures(mat_in, base_img, normal_imgp, mr_imgp, emissive_imgp,
					    occl_imgp, basecolp, normalp, mrp, emissivep);

	constexpr F32 VIRTUAL_SIZE = 64.f * 64.f;

	if (basecolp)
	{
		basecolp->addTextureStats(VIRTUAL_SIZE);
		matp->mTextureId[BASECOLIDX] = basecolp->getID();
	}
	else
	{
		matp->mTextureId[BASECOLIDX].setNull();
	}
	matp->mBaseColorTexture = basecolp;

	if (normalp)
	{
		normalp->addTextureStats(VIRTUAL_SIZE);
		matp->mTextureId[NORMALIDX] = normalp->getID();
	}
	else
	{
		matp->mTextureId[NORMALIDX].setNull();
	}
	matp->mNormalTexture = normalp;

	if (mrp)
	{
		mrp->addTextureStats(VIRTUAL_SIZE);
		matp->mTextureId[MROUGHIDX] = mrp->getID();
	}
	else
	{
		matp->mTextureId[MROUGHIDX].setNull();
	}
	matp->mMetallicRoughnessTexture = mrp;

	if (emissivep)
	{
		emissivep->addTextureStats(VIRTUAL_SIZE);
		matp->mTextureId[EMISSIVEIDX] = emissivep->getID();
	}
	else
	{
		matp->mTextureId[EMISSIVEIDX].setNull();
	}
	matp->mEmissiveTexture = emissivep;

	return true;
}
