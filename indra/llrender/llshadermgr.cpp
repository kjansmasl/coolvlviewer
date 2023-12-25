/**
 * @file llshadermgr.cpp
 * @brief Shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llshadermgr.h"

#include "lldir.h"
#include "llrender.h"

#if LL_DARWIN
# include "OpenGL/OpenGL.h"
#endif

#if LL_DEBUG
# define UNIFORM_ERRS llerrs
#else
# define UNIFORM_ERRS llwarns_once
#endif

// Static member variables
LLShaderMgr* LLShaderMgr::sInstance = NULL;
LLShaderMgr::shaders_map_t LLShaderMgr::sVertexShaderObjects;
LLShaderMgr::shaders_map_t LLShaderMgr::sFragmentShaderObjects;
LLShaderMgr::reserved_strings_t LLShaderMgr::sReservedAttribs;
LLShaderMgr::reserved_strings_t LLShaderMgr::sReservedUniforms;

LLShaderMgr::LLShaderMgr()
{
	sInstance = this;
}

LLShaderMgr::~LLShaderMgr()
{
	sInstance = NULL;
}

//static
LLShaderMgr* LLShaderMgr::getInstance()
{
	if (!sInstance)
	{
		llerrs << "This should already have been instantiated by the application !"
			   << llendl;
	}
	return sInstance;
}

bool LLShaderMgr::attachShaderFeatures(LLGLSLShader* shader)
{
	if (!shader)
	{
		llerrs << "NULL shader pointer passed !" << llendl;
	}

	LLShaderFeatures* features = &shader->mFeatures;
	if (features->attachNothing)
	{
		return true;
	}

	//////////////////////////////////////
	// Attach Vertex Shader Features First
	//////////////////////////////////////

	// Note: the order of shader object attaching is VERY IMPORTANT !
	if (features->calculatesAtmospherics)
	{
		if (!gUsePBRShaders && features->hasWaterFog)
		{
			if (!shader->attachVertexObject("windlight/atmosphericsVarsWaterV.glsl"))
			{
				return false;
			}
		}
		else if (!shader->attachVertexObject("windlight/atmosphericsVarsV.glsl"))
		{
			return false;
		}
	}

	if (features->calculatesLighting || features->calculatesAtmospherics)
	{
		if (!shader->attachVertexObject("windlight/atmosphericsHelpersV.glsl"))
		{
			return false;
		}
	}

	if (features->calculatesLighting)
	{
		if (features->isSpecular)
		{
			if (!shader->attachVertexObject("lighting/lightFuncSpecularV.glsl"))
			{
				return false;
			}
			if (!features->isAlphaLighting &&
				!shader->attachVertexObject("lighting/sumLightsSpecularV.glsl"))
			{
				return false;
			}
			if (!shader->attachVertexObject("lighting/lightSpecularV.glsl"))
			{
				return false;
			}
		}
		else
		{
			if (!shader->attachVertexObject("lighting/lightFuncV.glsl"))
			{
				return false;
			}
			if (!features->isAlphaLighting &&
				!shader->attachVertexObject("lighting/sumLightsV.glsl"))
			{
				return false;
			}
			if (!shader->attachVertexObject("lighting/lightV.glsl"))
			{
				return false;
			}
		}
	}

	// Note: the order of shader object attaching is VERY IMPORTANT !
	if (features->calculatesAtmospherics)
	{
		if (gUsePBRShaders &&
			// Note: "F" suffix is superfluous here, there is nothing fragment
			// specific in srgbF.
			!shader->attachVertexObject("environment/srgbF.glsl"))
		{
			return false;
		}
		if (!shader->attachVertexObject("windlight/atmosphericsFuncs.glsl"))
		{
			return false;
		}
		if (!shader->attachVertexObject("windlight/atmosphericsV.glsl"))
		{
			return false;
		}
	}

	if (features->hasSkinning &&
		!shader->attachVertexObject("avatar/avatarSkinV.glsl"))
	{
		return false;
	}

	if (features->hasObjectSkinning)
	{
		shader->mRiggedVariant = shader;
		if (!shader->attachVertexObject("avatar/objectSkinV.glsl"))
		{
			return false;
		}
	}

	if (gUsePBRShaders &&
		!shader->attachVertexObject("deferred/textureUtilV.glsl"))
	{
		return false;
	}

	///////////////////////////////////////
	// Attach Fragment Shader Features Next
	///////////////////////////////////////

	// Note: the order of shader object attaching is VERY IMPORTANT !

	if (gUsePBRShaders &&
		(features->hasSrgb || features->hasAtmospherics ||
		 features->calculatesAtmospherics || features->isDeferred) &&
		!shader->attachFragmentObject("environment/srgbF.glsl"))
	{
		return false;
	}

	if (gUsePBRShaders)
	{
		if (features->calculatesAtmospherics || features->hasGamma ||
			features->isDeferred)
		{
			if (!shader->attachFragmentObject("windlight/atmosphericsVarsF.glsl"))
			{
				return false;
			}
		}
	}
	else if (features->calculatesAtmospherics)
	{
		if (features->hasWaterFog)
		{
			if (!shader->attachFragmentObject("windlight/atmosphericsVarsWaterF.glsl"))
			{
				return false;
			}
		}
		else if (!shader->attachFragmentObject("windlight/atmosphericsVarsF.glsl"))
		{
			return false;
		}
	}

	if (features->calculatesLighting || features->calculatesAtmospherics)
	{
		if (!shader->attachFragmentObject("windlight/atmosphericsHelpersF.glsl"))
		{
			return false;
		}
	}

	// We want this BEFORE shadows and AO because those facilities use pos/norm
	// access
	if ((features->isDeferred || features->hasReflectionProbes) &&
		!shader->attachFragmentObject("deferred/deferredUtil.glsl"))
	{
		return false;
	}

	if ((features->hasScreenSpaceReflections ||
		 features->hasReflectionProbes) &&
		!shader->attachFragmentObject("deferred/screenSpaceReflUtil.glsl"))
	{
		return false;
	}

	if (features->hasShadows &&
		!shader->attachFragmentObject("deferred/shadowUtil.glsl"))
	{
		return false;
	}

	if (features->hasReflectionProbes &&
		!shader->attachFragmentObject("deferred/reflectionProbeF.glsl"))
	{
		return false;
	}

	if (features->hasAmbientOcclusion &&
		!shader->attachFragmentObject("deferred/aoUtil.glsl"))
	{
		return false;
	}

	if ((features->hasGamma || (gUsePBRShaders && features->isDeferred)) &&
		!shader->attachFragmentObject("windlight/gammaF.glsl"))
	{
		return false;
	}

	if (!gUsePBRShaders && features->hasSrgb &&
		!shader->attachFragmentObject("environment/srgbF.glsl"))
	{
		return false;
	}

	if (features->encodesNormal &&
		!shader->attachFragmentObject("environment/encodeNormF.glsl"))
	{
		return false;
	}

	if (features->hasAtmospherics || (gUsePBRShaders && features->isDeferred))
	{
		if (!shader->attachFragmentObject("windlight/atmosphericsFuncs.glsl"))
		{
			return false;
		}
		if (!shader->attachFragmentObject("windlight/atmosphericsF.glsl"))
		{
			return false;
		}
	}

	if (features->hasTransport && !gUsePBRShaders &&
		!shader->attachFragmentObject("windlight/transportF.glsl"))
	{
		return false;
	}

	// Note: the order of shader object attaching is VERY IMPORTANT !
	if (gUsePBRShaders)
	{
		if (features->hasAtmospherics &&
			!shader->attachFragmentObject("environment/waterFogF.glsl"))
		{
			return false;
		}
	}
	else if (features->hasWaterFog &&
			 !shader->attachFragmentObject("environment/waterFogF.glsl"))
	{
		return false;
	}

	if (features->hasLighting)
	{
		if (features->hasWaterFog && !gUsePBRShaders)
		{
			if (features->disableTextureIndex)
			{
				if (features->hasAlphaMask)
				{
					if (!shader->attachFragmentObject("lighting/lightWaterAlphaMaskNonIndexedF.glsl"))
					{
						return false;
					}
				}
				else if (!shader->attachFragmentObject("lighting/lightWaterNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else
			{
				if (features->hasAlphaMask)
				{
					if (!shader->attachFragmentObject("lighting/lightWaterAlphaMaskF.glsl"))
					{
						return false;
					}
				}
				else if (!shader->attachFragmentObject("lighting/lightWaterF.glsl"))
				{
					return false;
				}
				shader->mFeatures.mIndexedTextureChannels =
					llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
			}
		}
		else if (features->disableTextureIndex)
		{
			if (features->hasAlphaMask)
			{
				if (!shader->attachFragmentObject("lighting/lightAlphaMaskNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else if (!shader->attachFragmentObject("lighting/lightNonIndexedF.glsl"))
			{
				return false;
			}
		}
		else
		{
			if (features->hasAlphaMask)
			{
				if (!shader->attachFragmentObject("lighting/lightAlphaMaskF.glsl"))
				{
					return false;
				}
			}
			else if (!shader->attachFragmentObject("lighting/lightF.glsl"))
			{
				return false;
			}
			shader->mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
		}
	}
	// Note: the order of shader objects attaching is VERY IMPORTANT !
	else if (!gUsePBRShaders && features->isFullbright)
	{
		if (features->isShiny && features->hasWaterFog)
		{
			if (features->disableTextureIndex)
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightShinyWaterNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightShinyWaterF.glsl"))
				{
					return false;
				}
				shader->mFeatures.mIndexedTextureChannels =
					llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
			}
		}
		else if (features->hasWaterFog)
		{
			if (features->disableTextureIndex)
			{
				if (features->hasAlphaMask)
				{
					if (!shader->attachFragmentObject("lighting/lightFullbrightWaterNonIndexedAlphaMaskF.glsl"))
					{
						return false;
					}
				}
				else if (!shader->attachFragmentObject("lighting/lightFullbrightWaterNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else
			{
				if (features->hasAlphaMask)
				{
					if (!shader->attachFragmentObject("lighting/lightFullbrightWaterAlphaMaskF.glsl"))
					{
						return false;
					}
				}
				else if (!shader->attachFragmentObject("lighting/lightFullbrightWaterF.glsl"))
				{
					return false;
				}
				shader->mFeatures.mIndexedTextureChannels =
					llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
			}
		}
		else if (features->isShiny)
		{
			if (features->disableTextureIndex)
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightShinyNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightShinyF.glsl"))
				{
					return false;
				}
				shader->mFeatures.mIndexedTextureChannels =
					llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
			}
		}
		else if (features->disableTextureIndex)
		{
			if (features->hasAlphaMask)
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightNonIndexedAlphaMaskF.glsl"))
				{
					return false;
				}
			}
			else if (!shader->attachFragmentObject("lighting/lightFullbrightNonIndexedF.glsl"))
			{
				return false;
			}
		}
		else
		{
			if (features->hasAlphaMask)
			{
				if (!shader->attachFragmentObject("lighting/lightFullbrightAlphaMaskF.glsl"))
				{
					return false;
				}
			}
			else if (!shader->attachFragmentObject("lighting/lightFullbrightF.glsl"))
			{
				return false;
			}
			shader->mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
		}
	}
	// Note: the order of shader object attaching is VERY IMPORTANT !
	else if (!gUsePBRShaders && features->isShiny)
	{
		if (features->hasWaterFog)
		{
			if (features->disableTextureIndex)
			{
				if (!shader->attachFragmentObject("lighting/lightShinyWaterNonIndexedF.glsl"))
				{
					return false;
				}
			}
			else
			{
				if (!shader->attachFragmentObject("lighting/lightShinyWaterF.glsl"))
				{
					return false;
				}
				shader->mFeatures.mIndexedTextureChannels =
					llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
			}
		}
		else if (features->disableTextureIndex)
		{
			if (!shader->attachFragmentObject("lighting/lightShinyNonIndexedF.glsl"))
			{
				return false;
			}
		}
		else
		{
			if (!shader->attachFragmentObject("lighting/lightShinyF.glsl"))
			{
				return false;
			}
			shader->mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
		}
	}

	if (features->mIndexedTextureChannels <= 1)
	{
		if (!shader->attachVertexObject("objects/nonindexedTextureV.glsl"))
		{
			return false;
		}
	}
	else if (!shader->attachVertexObject("objects/indexedTextureV.glsl"))
	{
		return false;
	}

	return true;
}

//============================================================================
// Load Shader

static std::string get_shader_log(GLuint object)
{
	std::string res;

	// Get log length
	GLint length;
	glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
	if (length > 0)
	{
		// The log could be any size, so allocate appropriately
		GLchar* log = new GLchar[length];
		glGetShaderInfoLog(object, length, &length, log);
		res = std::string((char*)log);
		delete[] log;
	}

	// Intel log spam removal
	if (gGLManager.mIsIntel && res.find("No errors") == 0)
	{
		res.clear();
	}

	return res;
}

static std::string get_program_log(GLuint object)
{
	std::string res;

	// Get log length
	GLint length;
	glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
	if (length > 0)
	{
		// The log could be any size, so allocate appropriately
		GLchar* log = new GLchar[length];
		glGetProgramInfoLog(object, length, &length, log);
		res = std::string((char*)log);
		delete[] log;
	}

	// Intel log spam removal
	if (gGLManager.mIsIntel && res.find("No errors") == 0)
	{
		res.clear();
	}

	return res;
}

void LLShaderMgr::dumpObjectLog(bool is_program, GLuint ret, bool warns)
{
	std::string log = is_program ? get_program_log(ret) : get_shader_log(ret);
	if (log.length() > 0)
	{
		if (warns)
		{
			llwarns << log << llendl;
		}
		else
		{
			LL_DEBUGS("ShaderLoading") << log << LL_ENDL;
		}
	}
}

void LLShaderMgr::dumpShaderSource(U32 shader_count, GLchar** shader_text)
{
	llinfos << "\n";
	for (U32 i = 0; i < shader_count; ++i)
	{
		llcont << i << ": " << shader_text[i];
	}
	llcont << llendl;
}

GLuint LLShaderMgr::loadShaderFile(const std::string& filename,
								   S32& shader_level, U32 type,
								   LLGLSLShader::defines_map_t* defines,
								   S32 texture_index_channels)
{
#if LL_DARWIN
	// Ensure work-around for missing GLSL funcs gets propogated to feature
	// shader files (e.g. srgbF.glsl)
	if (defines)
	{
		(*defines)["OLD_SELECT"] = "1";
	}
#endif

	LL_DEBUGS("ShaderLoading") << "Loading shader file: " << filename
							   << " class " << shader_level << LL_ENDL;

	if (filename.empty())
	{
		return 0;
	}

	// Read in from file
	LLFILE* file = NULL;

	S32 try_gpu_class = shader_level;
	S32 gpu_class;

	// Find the most relevant file
	const std::string& prefix = getShaderDirPrefix();
	for (gpu_class = try_gpu_class; gpu_class > 0; --gpu_class)
	{
		// Search from the current GPU class down to class 1 to find the most
		// relevant shader
		std::string fname = prefix + llformat("%d", gpu_class) +
							LL_DIR_DELIM_STR + filename;
		file = LLFile::open(fname, "r");
		if (file)
		{
			LL_DEBUGS("ShaderLoading") << "Loading file: " << fname << LL_ENDL;
			break; // Done
		}
	}

	if (!file)
	{
		llwarns << "GLSL Shader file not found: " << filename << llendl;
		return 0;
	}

	bool found_header = false;
	std::vector<std::string> header, body;
	// We would not accept lines longer than 1024 characters
	char line[1024];
	while (fgets(line, 1024, file))
	{
		if (!found_header && strstr((const char*)line, "[EXTRA_CODE_HERE]"))
		{
			found_header = true;
			header.swap(body);
		}
		else
		{
			body.emplace_back(line);
		}
	}
	LLFile::close(file);

	S32 major_version = gGLManager.mGLSLVersionMajor;
	S32 minor_version = gGLManager.mGLSLVersionMinor;
	std::string glsl_version;
	if (major_version == 1 && minor_version < 30)
	{
		if (gUsePBRShaders)
		{
			// We should NEVER get here: OpenGL v3.1 is the minimum requirement
			// for PBR.
			llerrs << "Unsupported GLSL Version." << llendl;
		}

		if (minor_version < 10)
		{
			// We should NEVER get here: if major version is 1 and minor
			// version is less than 10, viewer should never attempt to use
			// shaders and continuing would result in undefined behavior.
			llerrs << "Unsupported GLSL Version." << llendl;
		}

		if (minor_version <= 19)
		{
			glsl_version = "#version 110\n";
			header.emplace_back("#define ATTRIBUTE attribute\n");
			header.emplace_back("#define VARYING varying\n");
			header.emplace_back("#define VARYING_FLAT varying\n");
		}
		else if (minor_version <= 29)
		{
			// Set version to 1.20
			glsl_version = "#version 120\n";
			header.emplace_back("#define FXAA_GLSL_120 1\n");
			if (gGLManager.mHasGpuShader4)
			{
				header.emplace_back("#define FXAA_FAST_PIXEL_OFFSET 1\n");
			}
			else
			{
				header.emplace_back("#define FXAA_FAST_PIXEL_OFFSET 0\n");
			}
			header.emplace_back("#define ATTRIBUTE attribute\n");
			header.emplace_back("#define VARYING varying\n");
			header.emplace_back("#define VARYING_FLAT varying\n");
		}
	}
	else
	{
		if (major_version >= 4)
		{
			// Set version to 400 or 420
			if (minor_version >= 20)
			{
				glsl_version = "#version 420\n";
			}
			else
			{
				glsl_version = "#version 400\n";
			}
			header.emplace_back("#define FXAA_GLSL_400 1\n");
		}
		else if (major_version == 3)
		{
			if (minor_version < 10)
			{
				glsl_version = "#version 300\n";
			}
			else if (minor_version <= 19)
			{
				glsl_version = "#version 310\n";
			}
			else if (minor_version <= 29)
			{
				glsl_version = "#version 320\n";
			}
			else
			{
				glsl_version = "#version 330\n";
			}
			header.emplace_back("#define FXAA_GLSL_130 1\n");
		}
		else
		{
			if (minor_version >= 40 || gUsePBRShaders)
			{
				glsl_version = "#version 140\n";
			}
			else
			{
				// Set version to 1.30
				glsl_version = "#version 130\n";
			}
			if (minor_version == 50 && gGLManager.mHasGpuShader5 &&
				!gUsePBRShaders)
			{
				header.emplace_back("#extension GL_ARB_gpu_shader5 : enable\n");
			}
			header.emplace_back("#define FXAA_GLSL_130 1\n");
			// Some implementations of GLSL 1.30 require integer precision be
			// explicitly declared
			header.emplace_back("precision mediump int;\n");
			header.emplace_back("precision highp float;\n");
		}

		if (!gUsePBRShaders)
		{
			header.emplace_back("#define DEFINE_GL_FRAGCOLOR 1\n");
			header.emplace_back("#define ATTRIBUTE in\n");

			if (type == GL_VERTEX_SHADER)
			{
				// "varying" state is "out" in a vertex program, "in" in a
				// fragment program ("varying" is deprecated after version
				// 1.20)
				header.emplace_back("#define VARYING out\n");
				header.emplace_back("#define VARYING_FLAT flat out\n");
			}
			else
			{
				header.emplace_back("#define VARYING in\n");
				header.emplace_back("#define VARYING_FLAT flat in\n");
			}

			// Backwards compatibility with legacy texture lookup syntax
			header.emplace_back("#define texture2D texture\n");
			header.emplace_back("#define textureCube texture\n");
			header.emplace_back("#define texture2DLod textureLod\n");
			header.emplace_back("#define shadow2D(a,b) vec2(texture(a,b))\n");

			if (major_version > 1 || minor_version >= 40)
			{
				// GLSL 1.40 replaces texture2DRect et al with texture
				header.emplace_back("#define texture2DRect texture\n");
				header.emplace_back("#define shadow2DRect(a,b) vec2(texture(a,b))\n");
			}
		}
	}

	// Use alpha float to store bit flags. See addDeferredAttachments() in
	// llpipeline.cpp, and frag_data[2] in shaders.
	if (gUsePBRShaders)
	{
		// ATMOS kill
		header.emplace_back("#define GBUFFER_FLAG_SKIP_ATMOS 0.0 \n");
		// Bit 0
		header.emplace_back("#define GBUFFER_FLAG_HAS_ATMOS 0.34\n");
		// Bit 1
		header.emplace_back("#define GBUFFER_FLAG_HAS_PBR 0.67\n");
		header.emplace_back("#define GET_GBUFFER_FLAG(flag) (abs(norm.w-flag)<0.1)\n");
	}

	// Used by the SMAA shader only (this is actually one same shader source
	// used in both a fragment and a vertex shader files, with VERTEX_SHADER
	// defining what is its actual usage). HB
	if (type == GL_VERTEX_SHADER)
	{
		header.emplace_back("#define VERTEX_SHADER 1\n");
	}

	// Copy preprocessor definitions into buffer
	if (defines)
	{
		for (LLGLSLShader::defines_map_t::iterator iter = defines->begin(),
												   end = defines->end();
			 iter != end; ++iter)
		{
			header.emplace_back("#define " + iter->first + " " + iter->second +
								"\n");
		}
	}

	// #define used to implement workarounds for ATI GLSL compiler bugs
	if (gGLManager.mIsAMD)
	{
		header.emplace_back("#define IS_AMD_CARD 1\n");
	}

	if (texture_index_channels > 0 && type == GL_FRAGMENT_SHADER)
	{
		// Use specified number of texture channels for indexed texture
		// rendering

		/* prepend shader code that looks like this:

		uniform sampler2D tex0;
		uniform sampler2D tex1;
		uniform sampler2D tex2;
		.
		.
		.
		uniform sampler2D texN;

		VARYING_FLAT ivec4 vary_texture_index;

		vec4 ret = vec4(1,0,1,1);

		vec4 diffuseLookup(vec2 texcoord)
		{
			switch (vary_texture_index.r))
			{
				case 0: ret = texture2D(tex0, texcoord); break;
				case 1: ret = texture2D(tex1, texcoord); break;
				case 2: ret = texture2D(tex2, texcoord); break;
				.
				.
				.
				case N: return texture2D(texN, texcoord); break;
			}

			return ret;
		}

		NOTE: 'texture2D' is replaced with 'texture' for PBR shaders. HB
		*/

		header.emplace_back("#define HAS_DIFFUSE_LOOKUP\n");

		// Uniform declaration
		for (S32 i = 0; i < texture_index_channels; ++i)
		{
			header.emplace_back(llformat("uniform sampler2D tex%d;\n", i));
		}

		if (texture_index_channels > 1)
		{
			if (gUsePBRShaders)
			{
				header.emplace_back("flat in int vary_texture_index;\n");
			}
			else
			{
				header.emplace_back("VARYING_FLAT int vary_texture_index;\n");
			}
		}

		header.emplace_back("vec4 diffuseLookup(vec2 texcoord)\n");
		header.emplace_back("{\n");

		const char* texture_fn = gUsePBRShaders ? "texture" : "texture2D";

		if (texture_index_channels == 1)
		{
			// Do not use flow control, that is silly
			header.emplace_back(llformat("\treturn %s(tex0, texcoord);\n",
										 texture_fn));
			header.emplace_back("}\n");
		}
		else if (major_version < 2 && minor_version < 30)
		{
			// We should never get here. Indexed texture rendering requires
			// GLSL 1.30 or later (for passing integers between vertex and
			// fragment shaders)
			llerrs << "Indexed texture rendering requires GLSL 1.30 or later."
				   << llendl;
		}
		// Switches are unreliable on some NVIDIA drivers.
		// *TODO: check to see if that decade-old affirmation is still true
		// nowadays... Perhaps via a debug setting ?  HB
		else if (gGLManager.mIsNVIDIA)
		{
			static const char* if_line =
				"\t%sif (vary_texture_index == %d) { return %s(tex%d, texcoord); }\n";
			for (S32 i = 0; i < texture_index_channels; ++i)
			{
				header.emplace_back(llformat(if_line, i > 0 ? "else " : "", i,
											 texture_fn, i));
			}
			header.emplace_back("\treturn vec4(1,0,1,1);\n");
			header.emplace_back("}\n");
		}
		else
		{
			static const char* case_line =
				"\t\tcase %d: return %s(tex%d, texcoord);\n";
			header.emplace_back("\tvec4 ret = vec4(1,0,1,1);\n");
			header.emplace_back("\tswitch (vary_texture_index)\n");
			header.emplace_back("\t{\n");

			// Switch body
			for (S32 i = 0; i < texture_index_channels; ++i)
			{
				header.emplace_back(llformat(case_line, i, texture_fn, i));
			}

			header.emplace_back("\t}\n");
			header.emplace_back("\treturn ret;\n");
			header.emplace_back("}\n");
		}
	}

	// We cannot have any shaders longer than 4096 lines...
	constexpr U32 MAX_SHADER_TEXT_SIZE = 4096;
	GLchar* text[MAX_SHADER_TEXT_SIZE];
	U32 count = 0;
	// #version must come first in the directives...
	text[count++] = (GLchar*)strdup(glsl_version.c_str());
	// Copy shader header text into memory
	for (U32 i = 0, lines = header.size();
		 i < lines && count < MAX_SHADER_TEXT_SIZE; ++i)
	{
		text[count++] = (GLchar*)strdup(header[i].c_str());
	}
	// Copy shader body text into memory
	for (U32 i = 0, lines = body.size();
		 i < lines && count < MAX_SHADER_TEXT_SIZE; ++i)
	{
		text[count++] = (GLchar*)strdup(body[i].c_str());
	}

	LL_DEBUGS("ShaderPreprocessing") << filename << " text:\n"
									 << "----------------------------------\n";
	for (U32 i = 0; i < count; ++i)
	{
		LL_CONT << text[i];
	}
	LL_CONT << "----------------------------------" << LL_ENDL;

	GLenum error = GL_NO_ERROR;
	if (count >= MAX_SHADER_TEXT_SIZE)
	{
		llwarns << "Shader file " << filename
				<< " is too large (more than 4096 lines): shader loading skipped."
				<< llendl;
		++error;	// Flag an error, we do not care which. HB
	}

	GLuint ret = 0;
	if (error == GL_NO_ERROR)
	{
		// Create the shader object
		clear_glerror();
		ret = glCreateShader(type);
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			llwarns << "GL error in glCreateShader: " << error
					<< " - Shader file: " << filename << llendl;
			if (ret)
			{
				glDeleteShader(ret); // We no longer need that handle
				ret = 0;
			}
			clear_glerror();
		}
	}

	// Load source
	if (ret)
	{
		glShaderSource(ret, count, (const GLchar**)text, NULL);
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			llwarns << "GL error in glShaderSource: " << error
					<< " - Shader file: " << filename << llendl;
			if (ret)
			{
				glDeleteShader(ret); // We no longer need that handle
				ret = 0;
			}
			clear_glerror();
		}
	}

	// Compile source
	if (ret)
	{
		glCompileShader(ret);
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			llwarns << "GL error in glCompileShader: " << error
					<< " - Shader file: " << filename << llendl;
			if (ret)
			{
				glDeleteShader(ret); // We no longer need that handle
				ret = 0;
			}
			clear_glerror();
		}
	}

	if (ret)
	{
		// Check for errors
		GLint success = GL_TRUE;
		glGetShaderiv(ret, GL_COMPILE_STATUS, &success);
		error = glGetError();
		if (error != GL_NO_ERROR || success == GL_FALSE)
		{
			// An error occured, print log
			llwarns << "GLSL compilation error: " << error
					<< " - Shader file: " << filename << llendl;
			if (gDebugGL)
			{
				dumpObjectLog(false, ret);
				dumpShaderSource(count, text);
			}
			glDeleteShader(ret); // We no longer need that handle
			ret = 0;
			clear_glerror();
		}
	}

	// Free memory
	for (GLuint i = 0; i < count; i++)
	{
		free(text[i]);
	}

	// Successfully loaded, save results
	if (ret)
	{
		// Add shader file to map
		if (type == GL_VERTEX_SHADER)
		{
			sVertexShaderObjects[filename] = ret;
		}
		else if (type == GL_FRAGMENT_SHADER)
		{
			sFragmentShaderObjects[filename] = ret;
		}
		else
		{
			llwarns << "Unmanaged shader type " << type << " for: "
					<< filename << llendl;
		}
		shader_level = try_gpu_class;
		return ret;
	}

	if (shader_level <= 1)
	{
		llwarns << "Failed to load " << filename << llendl;
		return ret;
	}

	// Try again at a lower shader level...
	return loadShaderFile(filename, --shader_level, type, defines,
						  texture_index_channels);
}

bool LLShaderMgr::linkProgramObject(GLuint obj, bool suppress_errors)
{
	// Check for errors
	glLinkProgram(obj);
	GLint success = GL_TRUE;
	glGetProgramiv(obj, GL_LINK_STATUS, &success);
	if (success == GL_FALSE && !suppress_errors)
	{
		// An error occured, print log
		llwarns << "GLSL linker error:" << llendl;
	}

#if !LL_DARWIN
	std::string log = get_program_log(obj);
	LLStringUtil::toLower(log);
	if (log.find("software") != std::string::npos)
	{
		llwarns << "GLSL linker: running in software:" << llendl;
		success = GL_FALSE;
		suppress_errors = false;
	}
#endif

	if (!suppress_errors)
	{
        dumpObjectLog(true, obj, !success);
	}

	return success != GL_FALSE;
}

bool LLShaderMgr::validateProgramObject(GLuint obj)
{
	// Check program validity against current GL
	glValidateProgram(obj);
	GLint success = GL_TRUE;
	glGetShaderiv(obj, GL_VALIDATE_STATUS, &success);
	if (success == GL_FALSE)
	{
		llwarns << "GLSL program not valid: " << llendl;
		dumpObjectLog(true, obj);
		return false;
	}

	dumpObjectLog(true, obj, false);
	return true;
}

//virtual
void LLShaderMgr::initAttribsAndUniforms()
{
	sReservedAttribs.clear();
	sReservedUniforms.clear();

	// MUST match order of enum in llvertexbuffer.h
	sReservedAttribs.emplace_back("position");
	sReservedAttribs.emplace_back("normal");
	sReservedAttribs.emplace_back("texcoord0");
	sReservedAttribs.emplace_back("texcoord1");
	sReservedAttribs.emplace_back("texcoord2");
	sReservedAttribs.emplace_back("texcoord3");
	sReservedAttribs.emplace_back("diffuse_color");
	sReservedAttribs.emplace_back("emissive");
	sReservedAttribs.emplace_back("tangent");
	sReservedAttribs.emplace_back("weight");
	sReservedAttribs.emplace_back("weight4");
	sReservedAttribs.emplace_back("clothing");
	sReservedAttribs.emplace_back("texture_index");

	// Matrix state
	sReservedUniforms.emplace_back("modelview_matrix");
	sReservedUniforms.emplace_back("projection_matrix");
	sReservedUniforms.emplace_back("inv_proj");
	sReservedUniforms.emplace_back("modelview_projection_matrix");
	sReservedUniforms.emplace_back("inv_modelview");
	sReservedUniforms.emplace_back("normal_matrix");
	sReservedUniforms.emplace_back("texture_matrix0");
	// Actually never used by shaders, but currently needed by the C++ code due
	// to NUM_MATRIX_MODES. *TODO: cleanup the code and get rid of this. HB 
	sReservedUniforms.emplace_back("texture_matrix1");
	sReservedUniforms.emplace_back("texture_matrix2");
	sReservedUniforms.emplace_back("texture_matrix3");

	sReservedUniforms.emplace_back("object_plane_s");
	sReservedUniforms.emplace_back("object_plane_t");
	llassert(sReservedUniforms.size() == size_t(OBJECT_PLANE_T + 1));

	// For PBR shaders only
	sReservedUniforms.emplace_back("texture_base_color_transform");
	sReservedUniforms.emplace_back("texture_normal_transform");
	sReservedUniforms.emplace_back("texture_metallic_roughness_transform");
	sReservedUniforms.emplace_back("texture_emissive_transform");
	llassert(sReservedUniforms.size() ==
				size_t(TEXTURE_EMISSIVE_TRANSFORM + 1));

	sReservedUniforms.emplace_back("viewport");

	sReservedUniforms.emplace_back("light_position");
	sReservedUniforms.emplace_back("light_direction");
	sReservedUniforms.emplace_back("light_attenuation");
	// For PBR shaders only
	sReservedUniforms.emplace_back("light_deferred_attenuation");

	sReservedUniforms.emplace_back("light_diffuse");
	sReservedUniforms.emplace_back("light_ambient");
	sReservedUniforms.emplace_back("light_count");
	sReservedUniforms.emplace_back("light");
	sReservedUniforms.emplace_back("light_col");
	sReservedUniforms.emplace_back("far_z");

	llassert(sReservedUniforms.size() == size_t(MULTI_LIGHT_FAR_Z + 1));

	// MUST match order in eGLSLReservedUniforms
	sReservedUniforms.emplace_back("proj_mat");
	sReservedUniforms.emplace_back("proj_p");
	sReservedUniforms.emplace_back("proj_n");
	sReservedUniforms.emplace_back("proj_origin");
	sReservedUniforms.emplace_back("proj_range");
	sReservedUniforms.emplace_back("proj_ambiance");
	sReservedUniforms.emplace_back("proj_shadow_idx");
	sReservedUniforms.emplace_back("shadow_fade");
	sReservedUniforms.emplace_back("proj_focus");
	sReservedUniforms.emplace_back("proj_lod");

	llassert(sReservedUniforms.size() == size_t(PROJECTOR_LOD + 1));

	sReservedUniforms.emplace_back("color");
	// For PBR shaders only
	sReservedUniforms.emplace_back("emissiveColor");
	sReservedUniforms.emplace_back("metallicFactor");
	sReservedUniforms.emplace_back("roughnessFactor");

	sReservedUniforms.emplace_back("diffuseMap");
	sReservedUniforms.emplace_back("altDiffuseMap");
	sReservedUniforms.emplace_back("specularMap");
	sReservedUniforms.emplace_back("emissiveMap");			// For PBR shaders
	sReservedUniforms.emplace_back("bumpMap");
	sReservedUniforms.emplace_back("bumpMap2");
	sReservedUniforms.emplace_back("environmentMap");
	// For PBR shaders only
	sReservedUniforms.emplace_back("sceneMap");
	sReservedUniforms.emplace_back("sceneDepth");
	sReservedUniforms.emplace_back("reflectionProbes");
	sReservedUniforms.emplace_back("irradianceProbes");

	sReservedUniforms.emplace_back("cloud_noise_texture");
	sReservedUniforms.emplace_back("cloud_noise_texture_next");
	sReservedUniforms.emplace_back("fullbright");
	sReservedUniforms.emplace_back("lightnorm");
	sReservedUniforms.emplace_back("sunlight_color");
	sReservedUniforms.emplace_back("ambient_color");
	sReservedUniforms.emplace_back("sky_hdr_scale");		// For PBR shaders
	sReservedUniforms.emplace_back("sky_sunlight_scale");	// For PBR shaders
	sReservedUniforms.emplace_back("sky_ambient_scale");	// For PBR shaders
	sReservedUniforms.emplace_back("blue_horizon");
	sReservedUniforms.emplace_back("blue_density");
	sReservedUniforms.emplace_back("haze_horizon");
	sReservedUniforms.emplace_back("haze_density");
	sReservedUniforms.emplace_back("cloud_shadow");
	sReservedUniforms.emplace_back("density_multiplier");
	sReservedUniforms.emplace_back("distance_multiplier");
	sReservedUniforms.emplace_back("max_y");
	sReservedUniforms.emplace_back("glow");
	sReservedUniforms.emplace_back("cloud_color");
	sReservedUniforms.emplace_back("cloud_pos_density1");
	sReservedUniforms.emplace_back("cloud_pos_density2");
	sReservedUniforms.emplace_back("cloud_scale");
	sReservedUniforms.emplace_back("gamma");
	sReservedUniforms.emplace_back("scene_light_strength");

	llassert(sReservedUniforms.size() == size_t(SCENE_LIGHT_STRENGTH + 1));

	sReservedUniforms.emplace_back("center");
	sReservedUniforms.emplace_back("size");
	sReservedUniforms.emplace_back("falloff");

	sReservedUniforms.emplace_back("box_center");
	sReservedUniforms.emplace_back("box_size");

	sReservedUniforms.emplace_back("minLuminance");
	sReservedUniforms.emplace_back("maxExtractAlpha");
	sReservedUniforms.emplace_back("lumWeights");
	sReservedUniforms.emplace_back("warmthWeights");
	sReservedUniforms.emplace_back("warmthAmount");
	sReservedUniforms.emplace_back("glowStrength");
	sReservedUniforms.emplace_back("glowDelta");
	sReservedUniforms.emplace_back("glowNoiseMap");			// For PBR shaders

	llassert(sReservedUniforms.size() == size_t(GLOW_NOISE_MAP + 1));

	sReservedUniforms.emplace_back("minimum_alpha");
	sReservedUniforms.emplace_back("emissive_brightness");

	sReservedUniforms.emplace_back("shadow_matrix");
	sReservedUniforms.emplace_back("env_mat");
	sReservedUniforms.emplace_back("shadow_clip");
	sReservedUniforms.emplace_back("sun_wash");
	sReservedUniforms.emplace_back("shadow_noise");
	sReservedUniforms.emplace_back("blur_size");
	sReservedUniforms.emplace_back("ssao_radius");
	sReservedUniforms.emplace_back("ssao_max_radius");
	sReservedUniforms.emplace_back("ssao_factor");
	sReservedUniforms.emplace_back("ssao_effect_mat");
	sReservedUniforms.emplace_back("screen_res");
	sReservedUniforms.emplace_back("near_clip");
	sReservedUniforms.emplace_back("shadow_offset");
	sReservedUniforms.emplace_back("shadow_bias");
	sReservedUniforms.emplace_back("spot_shadow_bias");
	sReservedUniforms.emplace_back("spot_shadow_offset");
	sReservedUniforms.emplace_back("sun_dir");
	sReservedUniforms.emplace_back("moon_dir");
	sReservedUniforms.emplace_back("shadow_res");
	sReservedUniforms.emplace_back("proj_shadow_res");
	sReservedUniforms.emplace_back("shadow_target_width");

	// For PBR shaders only
	sReservedUniforms.emplace_back("iterationCount");
	sReservedUniforms.emplace_back("rayStep");
	sReservedUniforms.emplace_back("distanceBias");
	sReservedUniforms.emplace_back("depthRejectBias");
	sReservedUniforms.emplace_back("glossySampleCount");
	sReservedUniforms.emplace_back("noiseSine");
	sReservedUniforms.emplace_back("adaptiveStepMultiplier");

	llassert(sReservedUniforms.size() ==
				size_t(DEFERRED_SSR_NOISE_SINE + 1));

	// For PBR shaders only
	sReservedUniforms.emplace_back("modelview_delta");
	sReservedUniforms.emplace_back("inv_modelview_delta");
	sReservedUniforms.emplace_back("cube_snapshot");

	sReservedUniforms.emplace_back("tc_scale");
	sReservedUniforms.emplace_back("rcp_screen_res");
	sReservedUniforms.emplace_back("rcp_frame_opt");
	sReservedUniforms.emplace_back("rcp_frame_opt2");

	sReservedUniforms.emplace_back("focal_distance");
	sReservedUniforms.emplace_back("blur_constant");
	sReservedUniforms.emplace_back("tan_pixel_angle");
	sReservedUniforms.emplace_back("magnification");
	sReservedUniforms.emplace_back("max_cof");
	sReservedUniforms.emplace_back("res_scale");
	sReservedUniforms.emplace_back("dof_width");
	sReservedUniforms.emplace_back("dof_height");

	sReservedUniforms.emplace_back("depthMap");
	sReservedUniforms.emplace_back("shadowMap0");
	sReservedUniforms.emplace_back("shadowMap1");
	sReservedUniforms.emplace_back("shadowMap2");
	sReservedUniforms.emplace_back("shadowMap3");
	sReservedUniforms.emplace_back("shadowMap4");
	sReservedUniforms.emplace_back("shadowMap5");

	llassert(sReservedUniforms.size() == size_t(DEFERRED_SHADOW5 + 1));

	sReservedUniforms.emplace_back("normalMap");
	sReservedUniforms.emplace_back("positionMap");
	sReservedUniforms.emplace_back("diffuseRect");
	sReservedUniforms.emplace_back("specularRect");
	sReservedUniforms.emplace_back("emissiveRect");	// For PBR shaders only
	sReservedUniforms.emplace_back("exposureMap");	// For PBR shaders only
	sReservedUniforms.emplace_back("brdfLut");		// For PBR shaders only
	sReservedUniforms.emplace_back("noiseMap");
	sReservedUniforms.emplace_back("lightFunc");
	sReservedUniforms.emplace_back("lightMap");
	sReservedUniforms.emplace_back("bloomMap");		// For EE shaders only
	sReservedUniforms.emplace_back("projectionMap");
	sReservedUniforms.emplace_back("norm_mat");

	sReservedUniforms.emplace_back("texture_gamma");

	sReservedUniforms.emplace_back("specular_color");
	sReservedUniforms.emplace_back("env_intensity");

	sReservedUniforms.emplace_back("matrixPalette");

	sReservedUniforms.emplace_back("screenTex");
	sReservedUniforms.emplace_back("screenDepth");	// For PBR shaders only
	sReservedUniforms.emplace_back("refTex");
	sReservedUniforms.emplace_back("eyeVec");
	sReservedUniforms.emplace_back("time");
	sReservedUniforms.emplace_back("waveDir1");
	sReservedUniforms.emplace_back("waveDir2");
	sReservedUniforms.emplace_back("lightDir");
	sReservedUniforms.emplace_back("specular");
	sReservedUniforms.emplace_back("waterFogColor");
	sReservedUniforms.emplace_back("waterFogColorLinear");	// For PBR shaders
	sReservedUniforms.emplace_back("waterFogDensity");
	sReservedUniforms.emplace_back("waterFogKS");
	sReservedUniforms.emplace_back("refScale");
	sReservedUniforms.emplace_back("waterHeight");
	sReservedUniforms.emplace_back("waterPlane");
	sReservedUniforms.emplace_back("normScale");
	sReservedUniforms.emplace_back("fresnelScale");
	sReservedUniforms.emplace_back("fresnelOffset");
	sReservedUniforms.emplace_back("blurMultiplier");
	sReservedUniforms.emplace_back("sunAngle");

	sReservedUniforms.emplace_back("camPosLocal");

	sReservedUniforms.emplace_back("gWindDir");
	sReservedUniforms.emplace_back("gSinWaveParams");
	sReservedUniforms.emplace_back("gGravity");

	sReservedUniforms.emplace_back("detail_0");
	sReservedUniforms.emplace_back("detail_1");
	sReservedUniforms.emplace_back("detail_2");
	sReservedUniforms.emplace_back("detail_3");
	sReservedUniforms.emplace_back("alpha_ramp");

	sReservedUniforms.emplace_back("origin");

	sReservedUniforms.emplace_back("display_gamma");

	sReservedUniforms.emplace_back("sun_size");
	sReservedUniforms.emplace_back("fog_color");

	sReservedUniforms.emplace_back("blend_factor");
	sReservedUniforms.emplace_back("no_atmo");		// For EE shaders only
	sReservedUniforms.emplace_back("moisture_level");
	sReservedUniforms.emplace_back("droplet_radius");
	sReservedUniforms.emplace_back("ice_level");
	sReservedUniforms.emplace_back("rainbow_map");
	sReservedUniforms.emplace_back("halo_map");
	sReservedUniforms.emplace_back("moon_brightness");
	sReservedUniforms.emplace_back("cloud_variance");

	// For PBR shaders only
	sReservedUniforms.emplace_back("reflection_probe_ambiance");
	sReservedUniforms.emplace_back("max_probe_lod");

	// Used only by the EE shaders, but not in the C++ renderer code.
	// *TODO: check for a possible bug or eliminate if actually useless. HB
	sReservedUniforms.emplace_back("sh_input_r");
	sReservedUniforms.emplace_back("sh_input_g");
	sReservedUniforms.emplace_back("sh_input_b");

	sReservedUniforms.emplace_back("sun_moon_glow_factor");
	sReservedUniforms.emplace_back("water_edge");	// For EE shaders only
	sReservedUniforms.emplace_back("sun_up_factor");
	sReservedUniforms.emplace_back("moonlight_color");

	llassert_always(sReservedUniforms.size() == (size_t)END_RESERVED_UNIFORMS);

	LL_DEBUGS("ShaderLoading") << "Checking duplicates in reserved uniforms: ";
	std::set<std::string> dupe_check;
	for (U32 i = 0, count = sReservedUniforms.size(); i < count; ++i)
	{
		if (dupe_check.find(sReservedUniforms[i]) != dupe_check.end())
		{
			LL_CONT << "Duplicate reserved uniform name found: "
					<< sReservedUniforms[i];
			llassert(false);
		}
		else
		{
			dupe_check.emplace(sReservedUniforms[i]);
		}
	}
	LL_CONT << "Done." << LL_ENDL;
}
