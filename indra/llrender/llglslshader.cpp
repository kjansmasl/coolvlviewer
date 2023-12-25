/**
 * @file llglslshader.cpp
 * @brief GLSL helper functions and state.
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

#include <utility>

#include "llglslshader.h"

#include "llshadermgr.h"
#include "llrendertarget.h"
#include "llvertexbuffer.h"

#if LL_DARWIN
# include "OpenGL/OpenGL.h"
#endif

GLuint LLGLSLShader::sCurBoundShader = 0;
LLGLSLShader* LLGLSLShader::sCurBoundShaderPtr = NULL;
S32 LLGLSLShader::sIndexedTextureChannels = 0;
bool LLGLSLShader::sProfileEnabled = false;
std::set<LLGLSLShader*> LLGLSLShader::sInstances;
U64 LLGLSLShader::sTotalTimeElapsed = 0;
U32 LLGLSLShader::sTotalTrianglesDrawn = 0;
U64 LLGLSLShader::sTotalSamplesDrawn = 0;
U32 LLGLSLShader::sTotalDrawCalls = 0;

// UI shaders
LLGLSLShader gUIProgram;
LLGLSLShader gSolidColorProgram;

// Shader constants: keep in sync with LLGLSLShader::EShaderConsts !
static const std::string sShaderConstsKey[LLGLSLShader::NUM_SHADER_CONSTS] =
{
	"LL_SHADER_CONST_CLOUD_MOON_DEPTH",
	"LL_SHADER_CONST_STAR_DEPTH"
};
static const std::string sShaderConstsVal[LLGLSLShader::NUM_SHADER_CONSTS] =
{
	"0.99998",	// SHADER_CONST_CLOUD_MOON_DEPTH
	"0.99999"	// SHADER_CONST_STAR_DEPTH
};

//-----------------------------------------------------------------------------
// LLShaderFeatures class
//-----------------------------------------------------------------------------

LLShaderFeatures::LLShaderFeatures()
:	mIndexedTextureChannels(0),
	calculatesLighting(false),
	calculatesAtmospherics(false),
	hasLighting(false),
	isAlphaLighting(false),
	isShiny(false),						// No more used by PBR shaders
	isFullbright(false),				// No more used by PBR shaders
	isSpecular(false),
	hasWaterFog(false),
	hasTransport(false),
	hasSkinning(false),
	hasObjectSkinning(false),
	hasAtmospherics(false),
	hasGamma(false),
	hasSrgb(false),
	encodesNormal(false),
	isDeferred(false),
	hasShadows(false),
	hasAmbientOcclusion(false),
	disableTextureIndex(false),
	hasAlphaMask(false),
	attachNothing(false),
	hasScreenSpaceReflections(false),	// For PBR shaders
	hasReflectionProbes(false)			// For PBR shaders
{
}

//-----------------------------------------------------------------------------
// LLGLSLShader class
//-----------------------------------------------------------------------------

//static
void LLGLSLShader::initProfile()
{
	sProfileEnabled = true;
	sTotalTimeElapsed = 0;
	sTotalTrianglesDrawn = 0;
	sTotalSamplesDrawn = 0;
	sTotalDrawCalls = 0;

	for (std::set<LLGLSLShader*>::iterator iter = sInstances.begin(),
										   end = sInstances.end();
		 iter != end; ++iter)
	{
		(*iter)->clearStats();
	}
}

struct LLGLSLShaderCompareTimeElapsed
{
	LL_INLINE bool operator()(const LLGLSLShader* const& lhs,
							  const LLGLSLShader* const& rhs)
	{
		return lhs->mTimeElapsed < rhs->mTimeElapsed;
	}
};

//static
void LLGLSLShader::finishProfile(bool emit_report)
{
	sProfileEnabled = false;

	if (!emit_report)
	{
		return;
	}

	std::vector<LLGLSLShader*> sorted;

	for (std::set<LLGLSLShader*>::iterator iter = sInstances.begin(),
										   end = sInstances.end();
		 iter != end; ++iter)
	{
		sorted.push_back(*iter);
	}

	std::sort(sorted.begin(), sorted.end(), LLGLSLShaderCompareTimeElapsed());

	for (std::vector<LLGLSLShader*>::iterator iter = sorted.begin(),
											  end = sorted.end();
		 iter != end; ++iter)
	{
		(*iter)->dumpStats();
	}

	llinfos << "\nTotal rendering time: "
			<< llformat("%.4f ms", sTotalTimeElapsed / 1000000.f)
			<< "\nTotal samples drawn: "
			<< llformat("%.4f million", sTotalSamplesDrawn / 1000000.f)
			<< "\nTotal triangles drawn: "
			<< llformat("%.3f million", sTotalTrianglesDrawn / 1000000.f)
			<< llendl;
}

void LLGLSLShader::clearStats()
{
	mTrianglesDrawn = 0;
	mTimeElapsed = 0;
	mSamplesDrawn = 0;
	mDrawCalls = 0;
}

void LLGLSLShader::dumpStats()
{
	if (mDrawCalls > 0)
	{
		llinfos << "\n=============================================\n"
				<< mName;
		for (U32 i = 0, count = mShaderFiles.size(); i < count; ++i)
		{
			llcont << "\n" << mShaderFiles[i].first;
		}
		llcont << "\n=============================================";

		F32 ms = mTimeElapsed / 1000000.f;
		F32 seconds = ms / 1000.f;

		F32 pct_tris = (F32)mTrianglesDrawn / (F32)sTotalTrianglesDrawn * 100.f;
		F32 tris_sec = (F32)(mTrianglesDrawn / 1000000.0);
		tris_sec /= seconds;

		F32 pct_samples = (F32)((F64)mSamplesDrawn / (F64)sTotalSamplesDrawn) * 100.f;
		F32 samples_sec = (F32)mSamplesDrawn / 1000000000.0;
		samples_sec /= seconds;

		F32 pct_calls = (F32) mDrawCalls / (F32)sTotalDrawCalls * 100.f;
		U32 avg_batch = mTrianglesDrawn / mDrawCalls;

		llcont << "\nTriangles Drawn: " << mTrianglesDrawn <<  " "
			   << llformat("(%.2f pct of total, %.3f million/sec)", pct_tris,
						   tris_sec )
			   << "\nDraw Calls: " << mDrawCalls << " "
			   << llformat("(%.2f pct of total, avg %d tris/call)", pct_calls,
						   avg_batch)
			   << "\nSamplesDrawn: " << mSamplesDrawn << " "
			   << llformat("(%.2f pct of total, %.3f billion/sec)",
						   pct_samples, samples_sec)
			   << "\nTime Elapsed: " << mTimeElapsed << " "
			   << llformat("(%.2f pct of total, %.5f ms)\n",
						   (F32)((F64)mTimeElapsed/(F64)sTotalTimeElapsed) * 100.f,
						   ms)
			   << llendl;
	}
}

void LLGLSLShader::placeProfileQuery()
{
	if (sProfileEnabled)
	{
		if (mTimerQuery == 0)
		{
			glGenQueries(1, &mSamplesQuery);
			glGenQueries(1, &mTimerQuery);
			glGenQueries(1, &mPrimitivesQuery);
		}

		glBeginQuery(GL_SAMPLES_PASSED, mSamplesQuery);
		glBeginQuery(GL_TIME_ELAPSED, mTimerQuery);
		glBeginQuery(GL_PRIMITIVES_GENERATED, mPrimitivesQuery);
	}
}

void LLGLSLShader::readProfileQuery()
{
	if (!sProfileEnabled)
	{
		return;
	}
	glEndQuery(GL_TIME_ELAPSED);
	glEndQuery(GL_SAMPLES_PASSED);
	glEndQuery(GL_PRIMITIVES_GENERATED);

	GLuint64 time_elapsed = 0;
	glGetQueryObjectui64v(mTimerQuery, GL_QUERY_RESULT, &time_elapsed);

	GLuint64 samples_passed = 0;
	glGetQueryObjectui64v(mSamplesQuery, GL_QUERY_RESULT, &samples_passed);
	stop_glerror();

	GLuint64 primitives = 0;
	glGetQueryObjectui64v(mPrimitivesQuery, GL_QUERY_RESULT, &primitives);

	sTotalTimeElapsed += time_elapsed;
	mTimeElapsed += time_elapsed;

	sTotalSamplesDrawn += samples_passed;
	mSamplesDrawn += samples_passed;

	U32 tri_count = (U32)primitives / 3;
	mTrianglesDrawn += tri_count;
	sTotalTrianglesDrawn += tri_count;

	++sTotalDrawCalls;
	++mDrawCalls;
}

LLGLSLShader::LLGLSLShader()
:	mProgramObject(0),
	mAttributeMask(0),
	mTotalUniformSize(0),
	mActiveTextureChannels(0),
	mShaderLevel(0),
	mShaderGroup(SG_DEFAULT),
	mUniformsDirty(true),
	mCanBindFast(false),
	mTimerQuery(0),
	mSamplesQuery(0),
	mPrimitivesQuery(0),
	mRiggedVariant(NULL)
{
}

void LLGLSLShader::setup(const char* name, S32 level,
						 const char* vertex_shader,
						 const char* fragment_shader)
{
	// NOTE: sadly, vertex shader names do not all end with "V.glsl", and
	// fragment shader names do not all end with "F.glsl", so only check for
	// contradictory naming...
	if (strstr(vertex_shader, "F.glsl"))
	{
		llerrs << "Passing a fragment shader name for the vertex shader: "
			   << vertex_shader << llendl;
	}
	if (strstr(fragment_shader, "V.glsl"))
	{
		llerrs << "Passing a vertex shader name for the fragment shader: "
			   << fragment_shader << llendl;
	}

	mName = name;
	mShaderLevel = level;
	mShaderFiles.clear();
	mShaderFiles.emplace_back(vertex_shader, GL_VERTEX_SHADER);
	mShaderFiles.emplace_back(fragment_shader, GL_FRAGMENT_SHADER);

	// Reset everything else to the default values

	mDefines.clear();

	mUniformsDirty = true;

	clearStats();

	mShaderGroup = SG_DEFAULT;
	mActiveTextureChannels = 0;
	mTimerQuery = 0;
	mSamplesQuery = 0;
	mAttributeMask = 0;
	mTotalUniformSize = 0;
	mCanBindFast = false;

	mFeatures.mIndexedTextureChannels = 0;
	mFeatures.calculatesLighting = mFeatures.calculatesAtmospherics =
		mFeatures.hasLighting = mFeatures.isAlphaLighting = mFeatures.isShiny =
		mFeatures.isFullbright = mFeatures.isSpecular = mFeatures.hasWaterFog =
		mFeatures.hasTransport = mFeatures.hasSkinning =
		mFeatures.hasObjectSkinning = mFeatures.hasAtmospherics =
		mFeatures.hasGamma = mFeatures.hasSrgb = mFeatures.encodesNormal =
		mFeatures.isDeferred = mFeatures.hasShadows =
		mFeatures.hasAmbientOcclusion = mFeatures.disableTextureIndex =
		mFeatures.hasAlphaMask = mFeatures.attachNothing =
		mFeatures.hasScreenSpaceReflections =
		mFeatures.hasReflectionProbes = false;
}

void LLGLSLShader::unload()
{
	mShaderFiles.clear();
	mDefines.clear();
	unloadInternal();
}

void LLGLSLShader::unloadInternal()
{
	sInstances.erase(this);

	clear_glerror();
	mAttribute.clear();
	mTexture.clear();
	mUniform.clear();

	if (mProgramObject)
	{
		GLuint obj[1024];
		GLsizei count;
		glGetAttachedShaders(mProgramObject, sizeof(obj) / sizeof(obj[0]),
							&count, obj);
		for (GLsizei i = 0; i < count; ++i)
		{
			glDetachShader(mProgramObject, obj[i]);
		}
		for (GLsizei i = 0; i < count; ++i)
		{
			if (glIsShader(obj[i]))
			{
				glDeleteShader(obj[i]);
			}
		}
		glDeleteProgram(mProgramObject);
		mProgramObject = 0;
	}

	if (mTimerQuery)
	{
		glDeleteQueries(1, &mTimerQuery);
		mTimerQuery = 0;
	}

	if (mSamplesQuery)
	{
		glDeleteQueries(1, &mSamplesQuery);
		mSamplesQuery = 0;
	}

#if LL_DARWIN
	// *HACK: to stop Apple complaining
	clear_glerror();
#else
	stop_glerror();
#endif
}

bool LLGLSLShader::createShader(hash_vector_t* attributes,
								hash_vector_t* uniforms, U32 varying_count,
								const char** varyings)
{
	unloadInternal();

	sInstances.insert(this);

	// Reloading, reset matrix hash values
	for (U32 i = 0; i < LLRender::NUM_MATRIX_MODES; ++i)
	{
		mMatHash[i] = 0xFFFFFFFF;
	}
	mLightHash = 0xFFFFFFFF;

	llassert_always(!mShaderFiles.empty());

	// Create program
	mProgramObject = glCreateProgram();
	if (mProgramObject == 0)
	{
		// This should not happen if shader-related extensions, like
		// ARB_vertex_shader, exist.
		llwarns << "Failed to create handle for shader: " << mName << llendl;
		return false;
	}

	bool success = true;

	LLShaderMgr* shadermgr = LLShaderMgr::getInstance();

#if LL_DARWIN
	// Work-around missing mix(vec3,vec3,bvec3)
	mDefines["OLD_SELECT"] = "1";
#endif

	// Compile new source
	for (files_map_t::iterator it = mShaderFiles.begin();
		 it != mShaderFiles.end(); ++it)
	{
		GLuint shaderhandle =
			shadermgr->loadShaderFile(it->first, mShaderLevel, it->second,
									  &mDefines,
									  mFeatures.mIndexedTextureChannels);
		llinfos << "Creating shader: " << mName << " - Level: " << mShaderLevel
				<< " - File: " << it->first << LL_ENDL;
		if (shaderhandle)
		{
			attachObject(shaderhandle);
		}
		else
		{
			success = false;
		}
	}

	// Attach existing objects
	if (!shadermgr->attachShaderFeatures(this))
	{
		unloadInternal();
		return false;
	}

	if (gGLManager.mGLSLVersionMajor < 2 && gGLManager.mGLSLVersionMinor < 3)
	{
		// Indexed texture rendering requires GLSL 1.3 or later
		// attachShaderFeatures may have set the number of indexed texture
		// channels, so set to 1 again
		mFeatures.mIndexedTextureChannels =
			llmin(mFeatures.mIndexedTextureChannels, 1);
	}

	// Map attributes and uniforms
	if (success)
	{
		success = mapAttributes(attributes);
	}
	else
	{
		llwarns << "Failed to map attributes for: " << mName << llendl;
	}
	if (success)
	{
		success = mapUniforms(uniforms);
	}
	else
	{
		llwarns << "Failed to map uniforms for: " << mName << llendl;
	}

	if (!success)
	{
		// Try again using a lower shader level;
		if (mShaderLevel > 0)
		{
			llwarns << "Failed to link using shader level "
					<< mShaderLevel << " trying again using shader level "
					<< mShaderLevel - 1 << llendl;
			--mShaderLevel;
			return createShader(attributes, uniforms);
		}
		llwarns << "Failed to link shader: " << mName << llendl;
		unloadInternal();
		return false;
	}

	if (mFeatures.mIndexedTextureChannels > 0)
	{
		// Override texture channels for indexed texture rendering
		bind();
		S32 channel_count = mFeatures.mIndexedTextureChannels;

		for (S32 i = 0; i < channel_count; ++i)
		{
			LLStaticHashedString uni_name(llformat("tex%d", i));
			uniform1i(uni_name, i);
		}

		// Adjust any texture channels that might have been overwritten
		S32 cur_tex = channel_count;

		for (U32 i = 0, count = mTexture.size(); i < count; ++i)
		{
			if (mTexture[i] > -1 && mTexture[i] < channel_count)
			{
				llassert(cur_tex < gGLManager.mNumTextureImageUnits);
				uniform1i(i, cur_tex);
				mTexture[i] = cur_tex++;
			}
		}
		unbind();
	}

	return true;
}

bool LLGLSLShader::attachVertexObject(const char* object)
{
	// NOTE: sadly, vertex shader names do not all end with "V.glsl", so only
	// check for contradictory naming... HB
	bool error = strstr(object, "F.glsl") != 0;
	// PBR shaders got environment/srgbF.glsl also used as a vertex shader, so
	// we must check for this silly exception... HB
	if (error && gUsePBRShaders && strstr(object, "srgbF.glsl"))
	{
		error = false;
	}
	if (error)
	{
		llerrs << "Passing a vertex shader name for a fragment shader: "
			   << object << llendl;
	}
	LL_DEBUGS("ShaderLoading") << "Attaching: " << object << LL_ENDL;
	LLShaderMgr::map_citer_t it =
		LLShaderMgr::sVertexShaderObjects.find(object);
	if (it != LLShaderMgr::sVertexShaderObjects.end())
	{
		stop_glerror();
		glAttachShader(mProgramObject, it->second);
		stop_glerror();
		return true;
	}

	llwarns << "Attempting to attach shader object that has not been compiled: "
			<< object << llendl;
	return false;
}

bool LLGLSLShader::attachFragmentObject(const char* object)
{
	// NOTE: sadly, fragment shader names do not all end with "F.glsl", so only
	// check for contradictory naming... HB
	if (strstr(object, "V.glsl"))
	{
		llerrs << "Passing a fragment shader name for a vertex shader: "
			   << object << llendl;
	}
	LL_DEBUGS("ShaderLoading") << "Attaching: " << object << LL_ENDL;
	LLShaderMgr::map_citer_t it =
		LLShaderMgr::sFragmentShaderObjects.find(object);
	if (it != LLShaderMgr::sFragmentShaderObjects.end())
	{
		stop_glerror();
		glAttachShader(mProgramObject, it->second);
		stop_glerror();
		return true;
	}

	llwarns << "Attempting to attach shader object that has not been compiled: "
			<< object << llendl;
	return false;
}

void LLGLSLShader::attachObject(GLuint object)
{
	if (!object)
	{
		llwarns << "Attempting to attach non existing shader object."
				<< llendl;
		return;
	}
	stop_glerror();
	glAttachShader(mProgramObject, object);
	stop_glerror();
}

void LLGLSLShader::attachObjects(GLuint* objects, S32 count)
{
	for (S32 i = 0; i < count; ++i)
	{
		attachObject(objects[i]);
	}
}

bool LLGLSLShader::mapAttributes(const hash_vector_t* attributes)
{
	// Before linking, make sure reserved attributes always have consistent
	// locations
	for (U32 i = 0, count = LLShaderMgr::sReservedAttribs.size();
		 i < count; ++i)
	{
		const char* name = LLShaderMgr::sReservedAttribs[i].c_str();
		glBindAttribLocation(mProgramObject, i, (const GLchar*)name);
	}

	// Link the program
	bool res = LLShaderMgr::getInstance()->linkProgramObject(mProgramObject,
															 false);

	mAttribute.clear();
	U32 num_attrs = attributes ? attributes->size() : 0;
	mAttribute.resize(LLShaderMgr::sReservedAttribs.size() + num_attrs, -1);

	// Read back channel locations
	if (res)
	{
		mAttributeMask = 0;

		// Read back reserved channels first
		for (U32 i = 0, count = LLShaderMgr::sReservedAttribs.size(); i < count;
			 ++i)
		{
			const char* name = LLShaderMgr::sReservedAttribs[i].c_str();
			S32 index = glGetAttribLocation(mProgramObject,
											(const GLchar*)name);
			if (index != -1)
			{
				mAttribute[i] = index;
				mAttributeMask |= 1 << i;
				LL_DEBUGS("ShaderLoading") << "Attribute " << name
										   << " assigned to channel " << index
										   << LL_ENDL;
			}
		}
		if (attributes)
		{
			U32 size = LLShaderMgr::sReservedAttribs.size();
			for (U32 i = 0; i < num_attrs; ++i)
			{
				const char* name = (*attributes)[i].String().c_str();
				S32 index = glGetAttribLocation(mProgramObject, name);
				if (index != -1)
				{
					mAttribute[size + i] = index;
					LL_DEBUGS("ShaderLoading") << "Attribute " << name
											   << " assigned to channel "
											   << index << LL_ENDL;
				}
			}
		}
	}

	return res;
}

void LLGLSLShader::mapUniform(S32 index, const hash_vector_t* uniforms)
{
	if (index == -1)
	{
		return;
	}

	GLenum type;
	GLsizei length;
	S32 size = -1;
	char name[1024];
	name[0] = '\0';

	glGetActiveUniform(mProgramObject, index, 1024, &length, &size, &type,
					   (GLchar*)name);
	if (size > 0)
	{
		switch (type)
		{
			case GL_FLOAT_VEC2:			size *= 2; break;
			case GL_FLOAT_VEC3:			size *= 3; break;
			case GL_FLOAT_VEC4:			size *= 4; break;
			case GL_DOUBLE:				size *= 2; break;
			case GL_DOUBLE_VEC2:		size *= 2; break;
			case GL_DOUBLE_VEC3:		size *= 6; break;
			case GL_DOUBLE_VEC4:		size *= 8; break;
			case GL_INT_VEC2:			size *= 2; break;
			case GL_INT_VEC3:			size *= 3; break;
			case GL_INT_VEC4:			size *= 4; break;
			case GL_UNSIGNED_INT_VEC2:	size *= 2; break;
			case GL_UNSIGNED_INT_VEC3:	size *= 3; break;
			case GL_UNSIGNED_INT_VEC4:	size *= 4; break;
			case GL_BOOL_VEC2:			size *= 2; break;
			case GL_BOOL_VEC3:			size *= 3; break;
			case GL_BOOL_VEC4:			size *= 4; break;
			case GL_FLOAT_MAT2:			size *= 4; break;
			case GL_FLOAT_MAT3:			size *= 9; break;
			case GL_FLOAT_MAT4:			size *= 16; break;
			case GL_FLOAT_MAT2x3:		size *= 6; break;
			case GL_FLOAT_MAT2x4:		size *= 8; break;
			case GL_FLOAT_MAT3x2:		size *= 6; break;
			case GL_FLOAT_MAT3x4:		size *= 12; break;
			case GL_FLOAT_MAT4x2:		size *= 8; break;
			case GL_FLOAT_MAT4x3:		size *= 12; break;
			case GL_DOUBLE_MAT2:		size *= 8; break;
			case GL_DOUBLE_MAT3:		size *= 18; break;
			case GL_DOUBLE_MAT4:		size *= 32; break;
			case GL_DOUBLE_MAT2x3:		size *= 12; break;
			case GL_DOUBLE_MAT2x4:		size *= 16; break;
			case GL_DOUBLE_MAT3x2:		size *= 12; break;
			case GL_DOUBLE_MAT3x4:		size *= 24; break;
			case GL_DOUBLE_MAT4x2:		size *= 16; break;
			case GL_DOUBLE_MAT4x3:		size *= 24; break;
		}
		mTotalUniformSize += size;
	}

	S32 location = glGetUniformLocation(mProgramObject, name);
	if (location == -1)
	{
		return;	// Not found. Nothing more to do.
	}

	// Chop off "[0]" so we can always access the first element of an array by
	// the array name
	char* is_array = strstr(name, "[0]");
	if (is_array)
	{
		is_array[0] = '\0';
	}

	LLStaticHashedString hashed_name(name);
	mUniformNameMap[location] = name;
	mUniformMap[hashed_name] = location;
	LL_DEBUGS("ShaderLoading") << "Uniform " << name << " is at location "
							   << location << LL_ENDL;

	// Find the index of this uniform
	U32 count = LLShaderMgr::sReservedUniforms.size();
	for (U32 i = 0; i < count; ++i)
	{
		if (mUniform[i] == -1 && LLShaderMgr::sReservedUniforms[i] == name)
		{
			// Found it
			mUniform[i] = location;
			mTexture[i] = mapUniformTextureChannel(location, type, size);
			return;
		}
	}

	if (!uniforms)
	{
		return;
	}
	for (U32 i = 0; i < uniforms->size(); ++i)
	{
		if (mUniform[i + count] == -1 && (*uniforms)[i].String() == name)
		{
			// Found it
			mUniform[i + count] = location;
			mTexture[i + count] = mapUniformTextureChannel(location, type,
														   size);
			return;
		}
	}
}

void LLGLSLShader::addConstant(EShaderConsts shader_const)
{
	mDefines[sShaderConstsKey[shader_const]] = sShaderConstsVal[shader_const];
}

void LLGLSLShader::addPermutation(const std::string& name,
								  const std::string& value)
{
	mDefines.emplace(name, value);
}

void LLGLSLShader::addPermutations(const defines_map_t& defines)
{
	for (defines_map_t::const_iterator it = defines.begin(),
									   end = defines.end();
		 it != end; ++it)
	{
		mDefines.emplace(it->first, it->second);
	}
}

S32 LLGLSLShader::mapUniformTextureChannel(S32 location, U32 type, S32 size)
{
	if (type != GL_SAMPLER_2D_MULTISAMPLE &&
		type != GL_SAMPLER_CUBE_MAP_ARRAY &&
		(type < GL_SAMPLER_1D || type > GL_SAMPLER_2D_RECT_SHADOW))
	{
		return -1;
	}

	S32 ret = mActiveTextureChannels;

	if (size <= 1)
	{
		// This is a texture
		glUniform1i(location, mActiveTextureChannels);
		LL_DEBUGS("ShaderLoading") << "Location " << location << " of type "
								   << type << " assigned to texture channel "
								   << mActiveTextureChannels << LL_ENDL;
		++mActiveTextureChannels;
	}
	else
	{
		// This is an array of textures: make sequential after this texture
		GLint channel[32];	// Only support up to 32 texture channels
		if (size > 32)
		{
			llwarns << "Too many channels (max is 32): " << size << llendl;
			llassert(false);
			size = 32;
		}
		for (S32 i = 0; i < size; ++i)
		{
			channel[i] = mActiveTextureChannels++;
		}
		glUniform1iv(location, size, channel);
		LL_DEBUGS("ShaderLoading") << "Assigned to texture channel "
								   << mActiveTextureChannels - size
								   << mActiveTextureChannels - 1 << LL_ENDL;
	}

	if (mActiveTextureChannels > 32)
	{
		llwarns << "Too many total texture channels (max is 32): "
				<< mActiveTextureChannels << llendl;
		llassert(false);
	}

	return ret;
}

bool LLGLSLShader::mapUniforms(const hash_vector_t* uniforms)
{
	mTotalUniformSize = 0;
	mActiveTextureChannels = 0;
	mUniform.clear();
	mUniformMap.clear();
	mUniformNameMap.clear();
	mTexture.clear();
	mValue.clear();
	// Initialize arrays
	U32 num_uniforms = uniforms ? uniforms->size() : 0;
	U32 size = num_uniforms + LLShaderMgr::sReservedUniforms.size();
	mUniform.resize(size, -1);
	mTexture.resize(size, -1);

	bind();

	// Get the number of active uniforms
	GLint active_count;
	glGetProgramiv(mProgramObject, GL_ACTIVE_UNIFORMS, &active_count);

	// This is part of code is temporary because as the final result the
	// mapUniform() should be rewritten. But it would need a lot of work to
	// avoid possible regressions.
	// The reason of this code is that SL engine is very sensitive to the fact
	// that "diffuseMap" should appear first as uniform parameter so it gains
	// the 0-"texture channel" index (see mapUniformTextureChannel() and
	// mActiveTextureChannels); it influences which texture matrix will be
	// updated during rendering.
	// The order of indexes of uniform variables is not defined and the GLSL
	// compilers may change it as they see fit, even if the "diffuseMap"
	// appears and is used first in the shader code.
	S32 diffuse_map = -1;
	S32 specular_map = -1;
	S32 bump_map = -1;
	S32 environment_map = -1;
	S32 altdiffuse_map = -1;
	S32 reflection_map = -1;

	static const char diffmapname[] = "diffuseMap";
	static const char specularmapname[] = "specularMap";
	static const char bumpmapname[] = "bumpMap";
	static const char envmapname[] = "environmentMap";
	static const char altdiffusename[] = "altDiffuseMap";
	static const char reflectionname[] = "reflectionMap";
	if (glGetUniformLocation(mProgramObject, diffmapname) != -1 &&
		(glGetUniformLocation(mProgramObject, specularmapname) != -1 ||
		 glGetUniformLocation(mProgramObject, bumpmapname) != -1 ||
		 glGetUniformLocation(mProgramObject, envmapname) != -1 ||
		 glGetUniformLocation(mProgramObject, altdiffusename) != -1 ||
		 (gUsePBRShaders &&
		  glGetUniformLocation(mProgramObject, reflectionname) != -1)))
	{
		char name[1024];
		GLenum type;
		GLsizei length;
		GLint size;
		for (S32 i = 0; i < active_count; ++i)
		{
			name[0] = '\0';
			glGetActiveUniform(mProgramObject, i, 1024, &length, &size, &type,
							   (GLchar*)name);

			if (diffuse_map == -1 && strcmp(name, diffmapname) == 0)
			{
				diffuse_map = i;
				if (specular_map != -1 && bump_map != -1 &&
					environment_map != -1 && altdiffuse_map != -1 &&
					(!gUsePBRShaders || reflection_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (specular_map == -1 && strcmp(name, specularmapname) == 0)
			{
				specular_map = i;
				if (diffuse_map != -1 && bump_map != -1 &&
					environment_map != -1 && altdiffuse_map != -1 &&
					(!gUsePBRShaders || reflection_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (bump_map == -1 && strcmp(name, bumpmapname) == 0)
			{
				bump_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					environment_map != -1 && altdiffuse_map != -1 &&
					(!gUsePBRShaders || reflection_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (environment_map == -1 && strcmp(name, envmapname) == 0)
			{
				environment_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					bump_map != -1 && altdiffuse_map != -1 &&
					(!gUsePBRShaders || reflection_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (altdiffuse_map == -1 && strcmp(name, altdiffusename) == 0)
			{
				altdiffuse_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					bump_map != -1 && environment_map != -1 &&
					(!gUsePBRShaders || reflection_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (gUsePBRShaders && reflection_map == -1 &&
					 strcmp(name, reflectionname) == 0)
			{
				reflection_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					bump_map != -1 && environment_map != -1 &&
					altdiffuse_map != -1)
				{
					break;	// We are done !
				}
			}
		}
		// Map uniforms in the proper order
		if (diffuse_map != -1)
		{
			mapUniform(diffuse_map, uniforms);
		}
		else
		{
			llwarns << "Diffuse map advertized but not found in program object "
					<< mProgramObject << " !" << llendl;
			llassert(false);
		}
		if (altdiffuse_map != -1)
		{
			mapUniform(altdiffuse_map, uniforms);
		}
		if (specular_map != -1)
		{
			mapUniform(specular_map, uniforms);
		}
		if (bump_map != -1)
		{
			mapUniform(bump_map, uniforms);
		}
		if (environment_map != -1)
		{
			mapUniform(environment_map, uniforms);
		}
		if (reflection_map != -1)
		{
			mapUniform(reflection_map, uniforms);
		}
	}

	for (S32 i = 0; i < active_count; ++i)
	{
		if (i != specular_map && i != bump_map && i != diffuse_map &&
			i != environment_map && i != altdiffuse_map && i != reflection_map)
		{
			mapUniform(i, uniforms);
		}
	}

	if (gUsePBRShaders && mFeatures.hasReflectionProbes)
	{
		// Set up block binding, in a way supported by Apple (rather than
		// binding = 1 in .glsl). See slide 35 and more of:
		// https://docs.huihoo.com/apple/wwdc/2011/
		//  session_420__advances_in_opengl_for_mac_os_x_lion.pdf
		constexpr GLuint BLOCKBINDING = 1; // Picked by us
		// Get the index, similar to a uniform location
		GLuint idx = glGetUniformBlockIndex(mProgramObject,
											"ReflectionProbes");
		if (idx != GL_INVALID_INDEX)
		{
			// Set this index to a binding index
			glUniformBlockBinding(mProgramObject, idx, BLOCKBINDING);
		}
	}

	unbind();

	LL_DEBUGS("ShaderLoading") << "Total Uniform Size: " << mTotalUniformSize
							   << LL_ENDL;

	return true;
}

void LLGLSLShader::bind()
{
	gGL.flush();

	if (sCurBoundShader != mProgramObject)
	{
		LLVertexBuffer::unbind();
		glUseProgram(mProgramObject);
		sCurBoundShader = mProgramObject;
		sCurBoundShaderPtr = this;
		if (gUsePBRShaders)
		{
			LLVertexBuffer::setupClientArrays(mAttributeMask);
		}
	}
	else if (gDebugGL)
	{
		llwarns_once << "Attempt to re-bind currently bound shader program: "
					 << mName << ". Ignored." << llendl;
	}

	if (mUniformsDirty)
	{
		LLShaderMgr::getInstance()->updateShaderUniforms(this);
		mUniformsDirty = false;
	}
}

void LLGLSLShader::bind(bool rigged)
{
	if (rigged)
	{
		if (mRiggedVariant)
		{
			mRiggedVariant->bind();
			return;
		}
		llwarns_once << "Shader " << mName << " is missing a rigged variant !"
					 << llendl;
	}
	bind();
}

void LLGLSLShader::unbind()
{
	gGL.flush();
	LLVertexBuffer::unbind();
	glUseProgram(0);
	sCurBoundShader = 0;
	sCurBoundShaderPtr = NULL;
}

S32 LLGLSLShader::getTexture(S32 line, S32 index)
{
	if (index < 0 || index >= (S32)mTexture.size())
	{
		llwarns_once << "Texture index out of range (" << index << ")";
		if (gDebugGL)
		{
			llcont << " at line " << line;
		}
		if (LLGLSLShader::sCurBoundShaderPtr)
		{
			llcont << " for bound shader: "
				   << LLGLSLShader::sCurBoundShaderPtr->mName;
		}
		llcont << llendl;
		llassert(false);
		return -1;
	}
	return mTexture[index];
}

S32 LLGLSLShader::bindTexture(S32 uniform, LLGLTexture* texp,
							  LLTexUnit::eTextureType mode,
							  LLTexUnit::eTextureColorSpace colorspace)
{
	S32 channel = getTexture(__LINE__, uniform);
	if (channel >= 0)
	{
		LLTexUnit* unitp = gGL.getTexUnit(channel);
		unitp->bindFast(texp);
		unitp->setTextureColorSpace(colorspace);
	}
	return channel;
}

// Used by the PBR renderer only.
S32 LLGLSLShader::bindTexture(S32 uniform, LLRenderTarget* targetp, bool depth,
							  LLTexUnit::eTextureFilterOptions mode, U32 index)
{
	S32 channel = getTexture(__LINE__, uniform);
	if (channel >= 0)
	{
		LLTexUnit* unitp = gGL.getTexUnit(channel);
		if (depth)
		{
			unitp->bind(targetp, true);
		}
		else
		{
			bool has_mips = mode == LLTexUnit::TFO_TRILINEAR ||
							mode == LLTexUnit::TFO_ANISOTROPIC;
			unitp->bindManual(targetp->getUsage(),
							  targetp->getTexture(index), has_mips);
		}
		unitp->setTextureFilteringOption(mode);
	}
	return channel;
}

S32 LLGLSLShader::enableTexture(S32 uniform, LLTexUnit::eTextureType mode,
								LLTexUnit::eTextureColorSpace colorspace)
{
	S32 channel = getTexture(__LINE__, uniform);
	if (channel >= 0)
	{
		LLTexUnit* unitp = gGL.getTexUnit(channel);		
		unitp->activate();
		unitp->enable(mode);
		unitp->setTextureColorSpace(colorspace);
	}
	return channel;
}

S32 LLGLSLShader::disableTexture(S32 uniform, LLTexUnit::eTextureType mode,
								 LLTexUnit::eTextureColorSpace colorspace)
{
	S32 channel = getTexture(__LINE__, uniform);
	if (channel >= 0)
	{
		LLTexUnit* unitp = gGL.getTexUnit(channel);
		if (unitp->getCurrType() != LLTexUnit::TT_NONE)
		{
			if (gDebugGL && unitp->getCurrType() != mode &&
				unitp->getCurColorSpace() != colorspace)
			{
				llwarns_once << "Texture channel " << channel
							 << " texture type corrupted." << llendl;
			}
			unitp->disable();
		}
	}
	return channel;
}

S32 LLGLSLShader::getUniform(S32 line, U32 index)
{
	if (index >= (U32)mUniform.size())
	{
		llwarns_once << "Uniform index out of range (" << index << ")";
		if (gDebugGL)
		{
			llcont << " at line " << line;
		}
		if (LLGLSLShader::sCurBoundShaderPtr)
		{
			llcont << " for bound shader: "
				   << LLGLSLShader::sCurBoundShaderPtr->mName;
		}
		llcont << llendl;
		llassert(false);
		return -1;
	}
	return mUniform[index];
}

void LLGLSLShader::uniform1i(U32 index, S32 x)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			if (iter == mValue.end() || iter->second.mV[0] != x)
			{
				glUniform1i(uniform, x);
				mValue[uniform] = LLVector4(x, 0.f, 0.f, 0.f);
			}
		}
	}
}

void LLGLSLShader::uniform1f(U32 index, F32 x)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			if (iter == mValue.end() || iter->second.mV[0] != x)
			{
				glUniform1f(uniform, x);
				mValue[uniform] = LLVector4(x, 0.f, 0.f, 0.f);
			}
		}
	}
}

void LLGLSLShader::uniform2f(U32 index, F32 x, F32 y)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(x, y, 0.f, 0.f);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform2f(uniform, x, y);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3f(U32 index, F32 x, F32 y, F32 z)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(x, y, z, 0.f);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform3f(uniform, x, y, z);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4f(U32 index, F32 x, F32 y, F32 z, F32 w)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(x, y, z, w);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform4f(uniform, x, y, z, w);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform1iv(U32 index, U32 count, const S32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], 0.f, 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform1iv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4iv(U32 index, U32 count, const S32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], v[1], v[2], v[3]);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform1iv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform1fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], 0.f, 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform1fv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform2fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], v[1], 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform2fv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], v[1], v[2], 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform3fv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(uniform);
			LLVector4 vec(v[0], v[1], v[2], v[3]);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform4fv(uniform, count, v);
				mValue[uniform] = vec;
			}
		}
	}
}

void LLGLSLShader::uniformMatrix2fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			glUniformMatrix2fv(uniform, count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix3fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			glUniformMatrix3fv(uniform, count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix3x4fv(U32 index, U32 count,
									  GLboolean transpose, const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			glUniformMatrix3x4fv(uniform, count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix4fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		S32 uniform = getUniform(__LINE__, index);
		if (uniform >= 0)
		{
			glUniformMatrix4fv(uniform, count, transpose, v);
		}
	}
}

S32 LLGLSLShader::getUniformLocation(const LLStaticHashedString& uniform)
{
	if (!mProgramObject)
	{
		return -1;
	}

	LLStaticStringTable<S32>::iterator iter = mUniformMap.find(uniform);
	if (iter == mUniformMap.end())
	{
		return -1;
	}

	if (gDebugGL)
	{
		stop_glerror();
		if (iter->second != glGetUniformLocation(mProgramObject,
												 uniform.String().c_str()))
		{
			llwarns_once << "Uniform does not match: "
						 << uniform.String().c_str() << llendl;
		}
	}

	return iter->second;
}

S32 LLGLSLShader::getUniformLocation(U32 index)
{
	if (mProgramObject && index < mUniform.size())
	{
		return mUniform[index];
	}
	return -1;
}

S32 LLGLSLShader::getAttribLocation(U32 attrib)
{
	return attrib < mAttribute.size() ? mAttribute[attrib] : -1;
}

void LLGLSLShader::uniform1i(const LLStaticHashedString& uniform, S32 v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v, 0.f, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform1i(location, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1iv(const LLStaticHashedString& uniform, U32 count,
							  const S32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], 0.f, 0.f, 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform1iv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform4iv(const LLStaticHashedString& uniform, U32 count,
							  const S32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], v[1], v[2], v[3]);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform4iv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2i(const LLStaticHashedString& uniform, S32 i, S32 j)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(i, j, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform2i(location, i, j);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1f(const LLStaticHashedString& uniform, F32 v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v, 0.f, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform1f(location, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2f(const LLStaticHashedString& uniform, F32 x, F32 y)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(x, y, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform2f(location, x,y);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform3f(const LLStaticHashedString& uniform, F32 x,
							 F32 y, F32 z)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(x, y, z, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform3f(location, x, y, z);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], 0.f, 0.f, 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform1fv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], v[1], 0.f, 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform2fv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform3fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], v[1], v[2], 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform3fv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform4fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		LLVector4 vec(v);
		uniform_value_map_t::iterator iter = mValue.find(location);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform4fv(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniformMatrix4fv(const LLStaticHashedString& uniform,
									U32 count, GLboolean transpose,
									const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		stop_glerror();
		glUniformMatrix4fv(location, count, transpose, v);
		stop_glerror();
	}
}

void LLGLSLShader::vertexAttrib4f(U32 index, F32 x, F32 y, F32 z, F32 w)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4f(mAttribute[index], x, y, z, w);
	}
}

void LLGLSLShader::vertexAttrib4fv(U32 index, F32* v)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4fv(mAttribute[index], v);
	}
}

void LLGLSLShader::setMinimumAlpha(F32 minimum)
{
	gGL.flush();
	uniform1f(LLShaderMgr::MINIMUM_ALPHA, minimum);
}

//-----------------------------------------------------------------------------
// LLShaderUniforms class
//-----------------------------------------------------------------------------

void LLShaderUniforms::apply(LLGLSLShader* shader)
{
	if (!mActive)
	{
		return;
	}
	for (U32 i = 0, count = mIntegers.size(); i < count; ++i)
	{
		const IntSetting& uniform = mIntegers[i];
		shader->uniform1i(uniform.mUniform, uniform.mValue);
	}
	for (U32 i = 0, count = mFloats.size(); i < count; ++i)
	{
		const FloatSetting& uniform = mFloats[i];
		shader->uniform1f(uniform.mUniform, uniform.mValue);
	}
	for (U32 i = 0, count = mVectors.size(); i < count; ++i)
	{
		const VectorSetting& uniform = mVectors[i];
		shader->uniform4fv(uniform.mUniform, 1, uniform.mValue.mV);
	}
	for (U32 i = 0, count = mVector3s.size(); i < count; ++i)
	{
		const Vector3Setting& uniform = mVector3s[i];
		shader->uniform3fv(uniform.mUniform, 1, uniform.mValue.mV);
	}
}
