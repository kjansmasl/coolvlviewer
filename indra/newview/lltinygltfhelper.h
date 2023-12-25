/**
 * @file lltinygltfhelper.h
 * @brief The LLTinyGLTFHelper class declaration
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

#ifndef LL_LLTINYGLTFHELPER_H
#define LL_LLTINYGLTFHELPER_H

#include "tinygltf/tiny_gltf.h"

#include "llgltfmaterial.h"
#include "llpointer.h"

#include "llgltfmateriallist.h"

class LLImageRaw;
class LLViewerFetchedTexture;

// Purely static class
class LLTinyGLTFHelper final
{
protected:
	LOG_CLASS(LLTinyGLTFHelper);

public:
	LLTinyGLTFHelper() = delete;
	~LLTinyGLTFHelper() = delete;

	static LLColor4 getColor(const std::vector<double>& in);
	static const tinygltf::Image* getImageFromTextureIndex(const tinygltf::Model& m,
														   S32 teX_idx);
	static LLImageRaw* getTexture(const std::string& folder,
								  const tinygltf::Model& model, S32 tex_idx,
								  std::string& name);
	static LLImageRaw* getTexture(const std::string& folder,
								  const tinygltf::Model& model, S32 tex_idx);

	static bool loadModel(const std::string& filename, tinygltf::Model& model_out);

	static bool getMaterialFromModel(const std::string& filename,
									 const tinygltf::Model& model, S32 mat_idx,
									 LLFetchedGLTFMaterial* materialp,
									 std::string& mat_name);

	static void initFetchedTextures(tinygltf::Material& materialp,
									LLPointer<LLImageRaw>& basecolor_imgp,
									LLPointer<LLImageRaw>& normal_imgp,
									LLPointer<LLImageRaw>& mr_imgp,
									LLPointer<LLImageRaw>& emissive_imgp,
									LLPointer<LLImageRaw>& occlusion_imgp,
									LLPointer<LLViewerFetchedTexture>& basecolorp,
									LLPointer<LLViewerFetchedTexture>& normalp,
									LLPointer<LLViewerFetchedTexture>& mrp,
									LLPointer<LLViewerFetchedTexture>& emissivep);
};

#endif	// LL_LLTINYGLTFHELPER_H
