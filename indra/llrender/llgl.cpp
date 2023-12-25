/**
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some
// useful functions for GL operations.

#define GLH_EXT_SINGLE_FILE

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llgl.h"

#include "llglslshader.h"
#include "llimagegl.h"
#include "llmath.h"
#include "llquaternion.h"
#include "llrender.h"
#include "llsys.h"
#include "llmatrix4.h"

#if LL_WINDOWS
# include "lldxhardware.h"
#endif

#define MAX_GL_TEXTURE_UNITS 16

bool gDebugGL = false;
// Global flag for dual-renderer support (EE/WL and PBR). HB
bool gUsePBRShaders = false;

std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

// Utility functions

void log_glerror(const char* file, U32 line, bool crash)
{
	// Do not call glGetError() while GL is stopped or not yet initialized. HB
	if (!gGL.isValid())
	{
		return;
	}

	static std::string filename;
	GLenum error = glGetError();
	if (LL_UNLIKELY(error))
	{
		filename.assign(file);
		size_t i = filename.find("indra");
		if (i != std::string::npos)
		{
			filename = filename.substr(i);
		}
	}
	while (LL_UNLIKELY(error))
	{
		std::string gl_error_msg = getGLErrorString(error);
		if (crash)
		{
			llerrs << "GL Error: " << gl_error_msg << " (" << error
				   << ") - in file: " << filename << " - at line: "
				   << line << llendl;
		}
		else
		{
			llwarns << "GL Error: " << gl_error_msg << " (" << error
					<< ") - in file: " << filename << " - at line: "
					<< line << llendl;
		}
		error = glGetError();
	}
}

// There are 7 non-zero error flags, one of them being cleared on each call to
// glGetError(). Normally, all error flags should therefore get cleared after
// at most 7 calls to glGetError() and the 8th call should always return 0...
// See: http://www.opengl.org/sdk/docs/man/xhtml/glGetError.xml
#define MAX_LOOPS 8U
void clear_glerror()
{
	// Do not call glGetError() while GL is stopped or not yet initialized. HB
	if (!gGL.isValid())
	{
		return;
	}

	U32 counter = MAX_LOOPS;
	if (LL_UNLIKELY(gDebugGL))
	{
		GLenum error;
		while ((error = glGetError()))
		{
			if (--counter == 0)
			{
				llwarns << "glGetError() still returning errors ("
						<< getGLErrorString(error) <<") after "
						<< MAX_LOOPS << " consecutive calls." << llendl;
				break;
			}
			else
			{
				llwarns << "glGetError() returned error: "
						<< getGLErrorString(error) << llendl;
			}
		}
	}
	else
	{
		// Fast code, for when gDebugGL is false
		while (glGetError() && --counter != 0) ;
	}
}

const std::string getGLErrorString(U32 error)
{
	switch (error)
	{
		case GL_NO_ERROR:
			return "GL_NO_ERROR";
			break;

		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
			break;

		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
			break;

		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
			break;

		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "GL_INVALID_FRAMEBUFFER_OPERATION";
			break;

		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
			break;

		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
			break;

		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
			break;

		default:
			return llformat("Unknown GL error #%d", error);
	}
}

static std::string parse_gl_version(S32& major, S32& minor, S32& release,
									std::string& vendor_specific)
{
	major = minor = release = 0;
	std::string version_string;

	// GL_VERSION returns a nul-terminated string with the format:
	// <major>.<minor>[.<release>] [<vendor specific>]
	const char* version = (const char*)glGetString(GL_VERSION);
	if (!version || !*version)
	{
		vendor_specific.clear();
		return version_string;
	}
	version_string.assign(version);

	std::string ver_copy(version);
	size_t len = strlen(version);
	size_t i = 0;
	size_t start;
	// Find the major version
	start = i;
	for ( ; i < len; ++i)
	{
		if (version[i] == '.')
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(major_str, major);

	if (version[i] == '.')
	{
		++i;
	}

	// Find the minor version
	start = i;
	for ( ; i < len; ++i)
	{
		if (version[i] == '.' || isspace(version[i]))
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(minor_str, minor);

	// Find the release number (optional)
	if (version[i] == '.')
	{
		++i;

		start = i;
		for ( ; i < len; ++i)
		{
			if (isspace(version[i]))
			{
				break;
			}
		}

		std::string release_str = ver_copy.substr(start, i - start);
		LLStringUtil::convertToS32(release_str, release);
	}

	// Skip over any white space
	while (version[i] && isspace(version[i]))
	{
		++i;
	}

	// Copy the vendor-specific string (optional)
	if (version[i])
	{
		vendor_specific.assign(version + i);
	}

	return version_string;
}

static void parse_glsl_version(S32& major, S32& minor)
{
	major = minor = 0;

	// GL_SHADING_LANGUAGE_VERSION returns a nul-terminated string with the
	// format: <major>.<minor>[.<release>] [<vendor specific>]
	const char* version =
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (!version)
	{
		return;
	}

	std::string ver_copy(version);
	size_t len = strlen(version);
	size_t i = 0;
	size_t start;
	// Find the major version
	start = i;
	for ( ; i < len; ++i)
	{
		if (version[i] == '.')
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(major_str, major);

	if (version[i] == '.')
	{
		i++;
	}

	// Find the minor version
	start = i;
	for ( ; i < len; ++i)
	{
		if (version[i] == '.' || isspace(version[i]))
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(minor_str, minor);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLManager class
///////////////////////////////////////////////////////////////////////////////

LLGLManager gGLManager;

LLGLManager::LLGLManager()
:	mInited(false),
	mIsDisabled(false),
#if LL_WINDOWS
	mHasAMDAssociations(false),
#endif
	mHasATIMemInfo(false),
	mHasNVXMemInfo(false),
	mMaxSamples(0),
	mHasSync(false),
	mHasVertexArrayObject(false),
	mNumTextureImageUnits(1),
	mMaxAnisotropy(1.f),
	mHasOcclusionQuery2(false),
	mHasTimerQuery(false),
	mHasDepthClamp(false),
	mHasAnisotropic(false),
	mHasCubeMapArray(false),
	mHasDebugOutput(false),
	mHasTextureSwizzle(false),
	mHasGpuShader4(false),
	mHasGpuShader5(false),
	mUseDepthClamp(false),
	mIsAMD(false),
	mIsNVIDIA(false),
	mIsIntel(false),
	mHasVertexAttribIPointer(false),
	mHasRequirements(true),
	mDriverVersionMajor(1),
	mDriverVersionMinor(0),
	mDriverVersionRelease(0),
	mGLVersion(1.f),
	mGLSLVersionMajor(0),
	mGLSLVersionMinor(0),
	mVRAM(0),
	mGLMaxVertexRange(0),
	mGLMaxIndexRange(0)
{
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
#if LL_WINDOWS
void LLGLManager::initWGL(HDC dc)
{
	if (!epoxy_has_wgl_extension(dc, "WGL_ARB_pixel_format"))
	{
		llwarns << "No ARB pixel format extensions" << llendl;
	}

	if (!epoxy_has_wgl_extension(dc, "WGL_ARB_create_context"))
	{
		llwarns << "No ARB create context extensions" << llendl;
	}

	mHasAMDAssociations =
		epoxy_has_wgl_extension(dc, "WGL_AMD_gpu_association");
}
#endif

// Return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
	if (mInited)	// Should never happen.
	{
		llerrs << "GL manager already initialized !" << llendl;
	}

	// Extract video card strings and convert to upper case to work around
	// driver-to-driver variation in capitalization.
	mGLVendor = ll_safe_string((const char*)glGetString(GL_VENDOR));
	LLStringUtil::toUpper(mGLVendor);

	mGLRenderer = ll_safe_string((const char*)glGetString(GL_RENDERER));
	LLStringUtil::toUpper(mGLRenderer);

	mGLVersionString = parse_gl_version(mDriverVersionMajor,
										mDriverVersionMinor,
										mDriverVersionRelease,
										mDriverVersionVendorString);

	mGLVersion = mDriverVersionMajor + mDriverVersionMinor * 0.1f;
	llinfos << "Advertised OpenGL version: " << mDriverVersionMajor << "."
			<< mDriverVersionMinor << llendl;

	// We do not support OpenGL below v2.0 any more.
	if (mGLVersion < 2.f)
	{
		mHasRequirements = false;
		llwarns << "Graphics driver is too old: OpenGL v2.0 minimum is required"
				<< llendl;
		return false;
	}

	parse_glsl_version(mGLSLVersionMajor, mGLSLVersionMinor);
	llinfos << "Advertised GLSL version: " << mGLSLVersionMajor << "."
			<< mGLSLVersionMinor << llendl;
#if LL_DARWIN
	// Limit the GLSL version to something compatible under macOS
	if (LLRender::sGLCoreProfile)
	{
		if (mGLVersion < 3.3f &&
			(mGLSLVersionMajor > 1 || mGLSLVersionMinor > 40))
		{
			mGLSLVersionMajor = 1;
			mGLSLVersionMinor = 40;
			llinfos << "Capped to GLSL v1.40." << llendl;
		}
	}
	else if (mGLSLVersionMajor > 1 || mGLSLVersionMinor > 20)
	{
		mGLSLVersionMajor = 1;
		mGLSLVersionMinor = 20;
		llinfos << "Capped to GLSL v1.20." << llendl;
	}
#endif
	// We do not use fixed GL functions any more so we need at the minimum
	// support for GLSL v1.10 so to load our basic shaders.
	if (mGLSLVersionMajor < 2 && mGLSLVersionMinor < 10)
	{
		mHasRequirements = false;
		llwarns << "Graphics driver is too old: GLSL v1.10 minimum is required"
				<< llendl;
		return false;
	}

	if (mGLVersion >= 2.1f && LLImageGL::sCompressTextures)
	{
		// Use texture compression
		glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
	}
	else
	{
		// Always disable texture compression
		LLImageGL::sCompressTextures = false;
	}

	if (mGLVendor.find("NVIDIA ") != std::string::npos)
	{
		mGLVendorShort = "NVIDIA";
		mIsNVIDIA = true;
	}
	else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
		 // The Mesa-based drivers put this in the Renderer string, not the
		// Vendor string.
		 || mGLRenderer.find("INTEL") != std::string::npos
#endif
		)
	{
		mGLVendorShort = "INTEL";
		mIsIntel = true;
	}
	// AMD is tested last, since there is more risks than with other vendors to
	// see the three letters composing the name appearing in another vendor's
	// GL driver name... HB
	// Trailing space necessary to keep "nVidia Corpor_ati_on" cards from being
	// recognized as ATI/AMD.
	// Note: AMD has been pretty good about not breaking this check, do not
	// rename without a good reason.
	else if (mGLVendor.substr(0, 4) == "ATI ")
	{
		mGLVendorShort = "AMD";
		mIsAMD = true;
	}
	else
	{
		mGLVendorShort = "MISC";
	}

	// This is called here because it may depend on above settings.
	initExtensions();

	if (!mHasRequirements)
	{
		// We do not support cards that do not support the
		// GL_ARB_framebuffer_object extension
		llwarns << "GL driver does not support GL_ARB_framebuffer_object"
				<< llendl;
		return false;
	}

	if (mHasAnisotropic)
	{
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &mMaxAnisotropy);
		mMaxAnisotropy = llmax(1.f, mMaxAnisotropy);
		llinfos << "Max anisotropy: " << mMaxAnisotropy << llendl;
	}

	S32 old_vram = mVRAM;
	mVRAM = mTexVRAM = 0;

#if LL_WINDOWS
	if (mHasAMDAssociations)
	{
		GLuint gl_gpus_count = wglGetGPUIDsAMD(0, 0);
		if (gl_gpus_count > 0)
		{
			GLuint* ids = new GLuint[gl_gpus_count];
			wglGetGPUIDsAMD(gl_gpus_count, ids);

			GLuint mem_mb = 0;
			for (U32 i = 0; i < gl_gpus_count; ++i)
			{
				wglGetGPUInfoAMD(ids[i], WGL_GPU_RAM_AMD, GL_UNSIGNED_INT,
								 sizeof(GLuint), &mem_mb);
				if (mVRAM < mem_mb)
				{
					// Basically pick the best AMD and trust driver/OS to know
					// to switch
					mVRAM = mem_mb;
				}
			}
		}
		if (mVRAM)
		{
			llinfos << "Detected VRAM via AMDAssociations: " << mVRAM
					<< llendl;
		}
	}
#endif

	if (mHasATIMemInfo)
	{
		// Ask GL how much VRAM is free for textures at startup
		GLint meminfo[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);
		mTexVRAM = meminfo[0] / 1024;
		llinfos << "Detected free VRAM for textures via ATIMemInfo: "
				<< mTexVRAM << " MB." << llendl;
	}
	else if (mHasNVXMemInfo)
	{
		GLint meminfo;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &meminfo);
		mVRAM = meminfo / 1024;
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX,
					  &meminfo);
		mTexVRAM = meminfo / 1024;
		llinfos << "Detected VRAM via NVXMemInfo: Total = " << mVRAM
				<< " MB - Free for textures: " << mTexVRAM << " MB." << llendl;
	}

#if LL_WINDOWS
	if (mVRAM < 256)
	{
		// Something likely went wrong using the above extensions...
		// Try via DXGI which will check all GPUs it knows of and will pick up
		// the one with most memory (i.e. we assume the most powerful one),
		// which will *likely* be the one the OS will pick up to render SL.
		S32 mem = LLDXHardware::getMBVideoMemoryViaDXGI();
		if (mem > 0)
		{
			mVRAM = mem;
			llinfos << "Detected VRAM via DXGI: " << mVRAM << llendl;
		}
	}
#endif

	if (mVRAM < 256)
	{
		if (old_vram > mVRAM)
		{
			// Fall back to old method
			mVRAM = old_vram;
		}
		else if (mTexVRAM > 0)
		{
			mVRAM = 4 * mTexVRAM / 3;
			llinfos << "Estimating total VRAM based on reported free VRAM for textures (this is inaccurate): "
					<< mVRAM << " MB." << llendl;
		}
	}

	if (mTexVRAM <= 0)
	{
		mTexVRAM = mVRAM / 2;
		llinfos << "Estimating usable VRAM for textures based on reported total VRAM (this is inaccurate): "
				<< mTexVRAM << " MB." << llendl;
	}

	stop_glerror();

	GLint num_tex_image_units;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &num_tex_image_units);
	mNumTextureImageUnits = llmin(num_tex_image_units, 32);

	if (LLRender::sGLCoreProfile)
	{
		if (mNumTextureImageUnits > MAX_GL_TEXTURE_UNITS)
		{
			mNumTextureImageUnits = MAX_GL_TEXTURE_UNITS;
		}
	}
	else 
	{
		GLint num_tex_units;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &num_tex_units);
		mNumTextureImageUnits = llmin(num_tex_units, MAX_GL_TEXTURE_UNITS);
		if (mIsIntel)
		{
			mNumTextureImageUnits = llmin(mNumTextureImageUnits, 2);
		}
	}

	stop_glerror();

	glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
	stop_glerror();

	initGLStates();

	return true;
}

void LLGLManager::initGLStates()
{
	LLGLState::initClass();
	stop_glerror();
}

void LLGLManager::getGLInfo(LLSD& info)
{
	info["GLInfo"]["GLVendor"] =
		ll_safe_string((const char*)glGetString(GL_VENDOR));
	info["GLInfo"]["GLRenderer"] =
		ll_safe_string((const char*)glGetString(GL_RENDERER));
	info["GLInfo"]["GLVersion"] =
		ll_safe_string((const char*)glGetString(GL_VERSION));
	std::string all_exts =
		ll_safe_string((const char*)glGetString(GL_EXTENSIONS));
	boost::char_separator<char> sep(" ");
	boost::tokenizer<boost::char_separator<char> > tok(all_exts, sep);
	for (boost::tokenizer<boost::char_separator<char> >::iterator
			i = tok.begin(); i != tok.end(); ++i)
	{
		info["GLInfo"]["GLExtensions"].append(*i);
	}
}

void LLGLManager::printGLInfoString()
{
	llinfos << "GL_VENDOR  : "
			<< ll_safe_string((const char*)glGetString(GL_VENDOR))
			<< llendl;
	llinfos << "GL_RENDERER: "
			<< ll_safe_string((const char*)glGetString(GL_RENDERER))
			<< llendl;
	llinfos << "GL_VERSION : "
			<< ll_safe_string((const char*)glGetString(GL_VERSION))
			<< llendl;
	std::string all_exts =
		ll_safe_string((const char*)glGetString(GL_EXTENSIONS));
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	LL_DEBUGS("RenderInit") << "GL_EXTENSIONS:\n" << all_exts << LL_ENDL;
}

std::string LLGLManager::getRawGLString()
{
	return ll_safe_string((char*)glGetString(GL_VENDOR)) + " " +
		   ll_safe_string((char*)glGetString(GL_RENDERER));
}

void LLGLManager::asLLSD(LLSD& info)
{
	// Currently these are duplicates of fields in LLViewerStats "system" info
	info["gpu_vendor"] = mGLVendorShort;
	info["gpu_version"] = mDriverVersionVendorString;
	info["opengl_version"] = mGLVersionString;
	info["gl_renderer"] = mGLRenderer;
	// Vendor
	info["is_ati"] = mIsAMD;
	info["is_intel"] = mIsIntel;
	info["is_nvidia"] = mIsNVIDIA;
	// Limits
	info["vram"] = mVRAM;
	info["num_texture_image_units"] =  mNumTextureImageUnits;
	info["max_samples"] = mMaxSamples;
	info["max_vertex_range"] = mGLMaxVertexRange;
	info["max_index_range"] = mGLMaxIndexRange;
	info["max_texture_size"] = mGLMaxTextureSize;
	// Extensions
	info["has_vertex_array_object"] = mHasVertexArrayObject;
	info["has_sync"] = mHasSync;
	info["has_timer_query"] = mHasTimerQuery;
	info["has_occlusion_query2"] = mHasOcclusionQuery2;
	info["has_depth_clamp"] = mHasDepthClamp;
	info["has_anisotropic"] = mHasAnisotropic;
	info["has_cubemap_array"] = mHasCubeMapArray;
	info["has_debug_output"] = mHasDebugOutput;
	info["has_nvx_mem_info"] = mHasNVXMemInfo;
	info["has_ati_mem_info"] = mHasATIMemInfo;
	// Got requirements for our renderer ?
	info["has_requirements"] = mHasRequirements;
}

void LLGLManager::shutdownGL()
{
	if (mInited)
	{
		glFinish();
		stop_glerror();
		mInited = false;
	}
}

void LLGLManager::initExtensions()
{
	mHasATIMemInfo = epoxy_has_gl_extension("GL_ATI_meminfo");
	mHasNVXMemInfo = epoxy_has_gl_extension("GL_NVX_gpu_memory_info");
	mHasAnisotropic = mGLVersion >= 4.6f ||
		epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic");
	mHasOcclusionQuery2 = mGLVersion >= 3.3f ||
						  epoxy_has_gl_extension("GL_ARB_occlusion_query2");
	mHasTimerQuery = mGLVersion >= 3.3f ||
					 epoxy_has_gl_extension("GL_ARB_timer_query");
	mHasVertexArrayObject = mGLVersion >= 3.f ||
		epoxy_has_gl_extension("GL_ARB_vertex_array_object");
	mHasSync = mGLVersion >= 3.2f || epoxy_has_gl_extension("GL_ARB_sync");
	mHasDepthClamp = mGLVersion >= 3.2f ||
					 epoxy_has_gl_extension("GL_ARB_depth_clamp") ||
					 epoxy_has_gl_extension("GL_NV_depth_clamp");
	if (!mHasDepthClamp)
	{
		mUseDepthClamp = false;
	}
	// Mask out FBO support when packed_depth_stencil is not there because we
	// need it for LLRenderTarget. Brad
#if GL_ARB_framebuffer_object
	mHasRequirements =
		mGLVersion >= 3.f ||
		epoxy_has_gl_extension("GL_ARB_framebuffer_object");
#else
	mHasRequirements =
		mGLVersion >= 3.f ||
		(epoxy_has_gl_extension("GL_EXT_framebuffer_object") &&
		 epoxy_has_gl_extension("GL_EXT_framebuffer_blit") &&
		 epoxy_has_gl_extension("GL_EXT_framebuffer_multisample") &&
		 epoxy_has_gl_extension("GL_EXT_packed_depth_stencil"));
#endif

	mHasCubeMapArray = mGLVersion >= 4.f;

	mHasDebugOutput = mGLVersion >= 4.3f ||
					  epoxy_has_gl_extension("GL_ARB_debug_output");

	mHasVertexAttribIPointer = mGLSLVersionMajor > 1 ||
							   mGLSLVersionMinor >= 30;

	mHasGpuShader4 = mGLVersion >= 3.f &&
					 epoxy_has_gl_extension("GL_ARB_gpu_shader4");
#if GL_ARB_gpu_shader5
	mHasGpuShader5 = epoxy_has_gl_extension("GL_ARB_gpu_shader5");
#endif

#if GL_ARB_texture_swizzle
	mHasTextureSwizzle = mGLVersion >= 3.3f ||
						 epoxy_has_gl_extension("GL_ARB_texture_swizzle");
#endif

	if (!mHasSync)
	{
		llinfos << "This GL implementation lacks GL_ARB_sync" << llendl;
	}
	if (!mHasAnisotropic)
	{
		llinfos << "Could not initialize anisotropic filtering" << llendl;
	}
	if (!mHasOcclusionQuery2)
	{
		llinfos << "Could not initialize GL_ARB_occlusion_query2" << llendl;
	}
	// Note: GL_ARB_vertex_array_object should exist in core GL profile v3.0+
	if (!mHasVertexArrayObject && LLRender::sGLCoreProfile)
	{
		llinfos << "Could not initialize GL_ARB_vertex_array_object" << llendl;
	}

	// Misc
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*)&mGLMaxVertexRange);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*)&mGLMaxIndexRange);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&mGLMaxTextureSize);

	mInited = true;

	clear_glerror();
}

///////////////////////////////////////////////////////////////////////////////
// LLGLState class
///////////////////////////////////////////////////////////////////////////////

// Static members
LLGLState::state_map_t LLGLState::sStateMap;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
U32 LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLState::initClass()
{
	sStateMap[GL_DITHER] = GL_TRUE;
	// Make sure multisample defaults to disabled
	sStateMap[GL_MULTISAMPLE] = GL_FALSE;
	glDisable(GL_MULTISAMPLE);
}

//static
void LLGLState::restoreGL()
{
	sStateMap.clear();
	initClass();
}

#if 0	// Not used, but kept in source, just in case... HB
//static
void LLGLState::resetTextureStates()
{
	gGL.flush();

	GLint max_tex_units;
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &max_tex_units);

	for (S32 j = max_tex_units - 1; j >= 0; --j)
	{
		LLTexUnit* unitp = gGL.getTexUnit(j);
		unitp->activate();
		glClientActiveTexture(GL_TEXTURE0 + j);
		if (j == 0)
		{
			unitp->enable(LLTexUnit::TT_TEXTURE);
		}
		else
		{
			unitp->disable();
		}
	}
}
#endif

void LLGLState::dumpStates()
{
	llinfos << "GL States:";
	for (state_map_t::iterator iter = sStateMap.begin(), end = sStateMap.end();
		 iter != end; ++iter)
	{
		llcont << llformat("\n   0x%04x : %s", (S32)iter->first,
						   iter->second ? "true" : "false");
	}
	llcont << llendl;
}

//static
void LLGLState::checkStates(const std::string& msg, S32 line)
{
	if (!gDebugGL)
	{
		return;
	}
	stop_glerror();

	static std::string errors;

	if (glIsEnabled(GL_BLEND))
	{
		GLint src;
		glGetIntegerv(GL_BLEND_SRC, &src);
		GLint dst;
		glGetIntegerv(GL_BLEND_DST, &dst);
		if (src != GL_SRC_ALPHA || dst != GL_ONE_MINUS_SRC_ALPHA)
		{
			errors =
				llformat("Blend function corrupted: source: 0x%04x, destination: 0x%04x",
						 src, dst);
		}
	}

	bool has_state_error = false;
	for (state_map_t::iterator iter = sStateMap.begin(), end = sStateMap.end();
		 iter != end; ++iter)
	{
		U32 state = iter->first;
		GLboolean cur_state = iter->second;
		GLboolean gl_state = glIsEnabled(state);
		if (cur_state != gl_state)
		{
			has_state_error = true;
			if (!errors.empty())
			{
				errors.append(" - ");
			}
			errors += llformat("Incoherent state: 0x%04x", state);
		}
	}
	if (has_state_error)
	{
		dumpStates();
	}

	if (!errors.empty())
	{
		llwarns << errors;
		if (!msg.empty())
		{
			llcont << " - " << msg;
		}
		if (line > 0)
		{
			llcont << " - line " << line;
		}
		llcont << llendl;
		errors.clear();
	}
}

LLGLState::LLGLState(U32 state, S32 enabled)
:	mState(state),
	mWasEnabled(GL_FALSE),
	mIsEnabled(GL_FALSE)
{
	// Always ignore any state deprecated post GL 3.0
	switch (state)
	{
		case GL_STENCIL_TEST:
			if (gUsePBRShaders)
			{
				llerrs << "GL_STENCIL_TEST used in PBR rendering mode !"
					   << llendl;
			}
			break;

		case GL_ALPHA_TEST:
		case GL_NORMALIZE:
		case GL_TEXTURE_GEN_R:
		case GL_TEXTURE_GEN_S:
		case GL_TEXTURE_GEN_T:
		case GL_TEXTURE_GEN_Q:
		case GL_LIGHTING:
		case GL_COLOR_MATERIAL:
		case GL_FOG:
		case GL_LINE_STIPPLE:
		case GL_POLYGON_STIPPLE:
			mState = 0;
			llwarns_once << "Asked for a deprecated GL state: " << state
						 << llendl;
			llassert(false);
	}

	if (mState)
	{
		mWasEnabled = sStateMap[state];
		setEnabled(enabled);
		stop_glerror();
	}
}

void LLGLState::setEnabled(S32 enabled)
{
	stop_glerror();
	if (!mState)
	{
		return;
	}
	if (enabled == CURRENT_STATE)
	{
		enabled = sStateMap[mState] == GL_TRUE ? GL_TRUE : GL_FALSE;
	}
	else if (enabled == GL_TRUE && sStateMap[mState] != GL_TRUE)
	{
		gGL.flush();
		glEnable(mState);
		sStateMap[mState] = GL_TRUE;
		stop_glerror();
	}
	else if (enabled == GL_FALSE && sStateMap[mState] != GL_FALSE)
	{
		gGL.flush();
		glDisable(mState);
		sStateMap[mState] = GL_FALSE;
		stop_glerror();
	}
	mIsEnabled = enabled;
}

//virtual
LLGLState::~LLGLState()
{
	if (mState)
	{
		if (gDebugGL)
		{
			GLboolean state = glIsEnabled(mState);
			if (sStateMap[mState] != state)
			{
				llwarns_once << "Mismatch for state: " << std::hex << mState
							 << std::dec << " - Actual status: " << state
							 << " (should be " << sStateMap[mState] << ")."
							 << llendl;
			}
		}
		if (mIsEnabled != mWasEnabled)
		{
			gGL.flush();
			if (mWasEnabled)
			{
				glEnable(mState);
				sStateMap[mState] = GL_TRUE;
			}
			else
			{
				glDisable(mState);
				sStateMap[mState] = GL_FALSE;
			}
			stop_glerror();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGLUserClipPlane class
///////////////////////////////////////////////////////////////////////////////

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const LLMatrix4a& mdlv,
									 const LLMatrix4a& proj, bool apply)
:	mApply(apply)
{
	if (apply)
	{
		mModelview = mdlv;
		mProjection = proj;
		// Flip incoming LLPlane to get consistent behavior compared to frustum
		// culling
		setPlane(-p[0], -p[1], -p[2], -p[3]);
	}
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
	disable();
}

void LLGLUserClipPlane::disable()
{
	if (mApply)
	{
		mApply = false;
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
	LLMatrix4a& p = mProjection;
	LLMatrix4a& m = mModelview;

	LLMatrix4a invtrans_mdlv;
	invtrans_mdlv.setMul(p, m);
	invtrans_mdlv.invert();
	invtrans_mdlv.transpose();

	LLVector4a oplane(a, b, c, d);
	LLVector4a cplane, cplane_splat, cplane_neg;

	invtrans_mdlv.rotate4(oplane, cplane);

	cplane_splat.splat<2>(cplane);
	cplane_splat.setAbs(cplane_splat);
	cplane.div(cplane_splat);
	cplane.sub(LLVector4a(0.f, 0.f, 0.f, 1.f));

	cplane_splat.splat<2>(cplane);
	cplane_neg = cplane;
	cplane_neg.negate();

	cplane.setSelectWithMask(cplane_splat.lessThan(_mm_setzero_ps()),
							 cplane_neg, cplane);

	LLMatrix4a suffix;
	suffix.setIdentity();
	suffix.setColumn<2>(cplane);
	LLMatrix4a new_proj;
	new_proj.setMul(suffix, p);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(new_proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLDepthTest class
///////////////////////////////////////////////////////////////////////////////

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled,
							 U32 depth_func, bool ignored)
:	mPrevDepthEnabled(sDepthEnabled),
	mPrevDepthFunc(sDepthFunc),
	mPrevWriteEnabled(sWriteEnabled),
	mIgnored(ignored)
{
	if (ignored)
	{
		// Do nothing.
		return;
	}

	stop_glerror();
	checkState();

	if (!depth_enabled)
	{
		// Always disable depth writes if depth testing is disabled. GL spec
		// defines this as a requirement, but some implementations allow depth
		// writes with testing disabled. The proper way to write to depth
		// buffer with testing disabled is to enable testing and use a
		// depth_func of GL_ALWAYS
		write_enabled = GL_FALSE;
	}

	if (depth_enabled != sDepthEnabled)
	{
		gGL.flush();
		if (depth_enabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
		sDepthEnabled = depth_enabled;
	}
	if (depth_func != sDepthFunc)
	{
		gGL.flush();
		glDepthFunc(depth_func);
		sDepthFunc = depth_func;
	}
	if (write_enabled != sWriteEnabled)
	{
		gGL.flush();
		glDepthMask(write_enabled);
		sWriteEnabled = write_enabled;
	}
	stop_glerror();
}

LLGLDepthTest::~LLGLDepthTest()
{
	if (mIgnored)
	{
		// Nothing to do.
		return;
	}

	checkState();
	if (sDepthEnabled != mPrevDepthEnabled)
	{
		gGL.flush();
		if (mPrevDepthEnabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
		sDepthEnabled = mPrevDepthEnabled;
	}
	if (sDepthFunc != mPrevDepthFunc)
	{
		gGL.flush();
		glDepthFunc(mPrevDepthFunc);
		sDepthFunc = mPrevDepthFunc;
	}
	if (sWriteEnabled != mPrevWriteEnabled)
	{
		gGL.flush();
		glDepthMask(mPrevWriteEnabled);
		sWriteEnabled = mPrevWriteEnabled;
	}
	stop_glerror();
}

void LLGLDepthTest::checkState()
{
	if (gDebugGL && !mIgnored)
	{
		GLint func = 0;
		GLboolean mask = GL_FALSE;

		glGetIntegerv(GL_DEPTH_FUNC, &func);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

		if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
			sWriteEnabled != mask || (GLint)sDepthFunc != func)
		{
			llwarns << "Unexpected depth testing state." << llendl;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGLSquashToFarClip class
///////////////////////////////////////////////////////////////////////////////

LLGLSquashToFarClip::LLGLSquashToFarClip(U32 layer)
{
	F32 depth = 0.99999f - 0.0001f * layer;
	LLMatrix4a proj = gGLProjection;
	LLVector4a col = proj.getColumn<3>();
	col.mul(depth);
	proj.setColumn<2>(col);

	U32 last_matrix_mode = gGL.getMatrixMode();
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(last_matrix_mode);
}

LLGLSquashToFarClip::~LLGLSquashToFarClip()
{
	U32 last_matrix_mode = gGL.getMatrixMode();
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(last_matrix_mode);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLSPipeline*SkyBox classes
///////////////////////////////////////////////////////////////////////////////

LLGLSPipelineSkyBox::LLGLSPipelineSkyBox()
:	mCullFace(GL_CULL_FACE),
	mSquashClip()
{
}

LLGLSPipelineDepthTestSkyBox::LLGLSPipelineDepthTestSkyBox(GLboolean depth_test,
														   GLboolean depth_write)
:	LLGLSPipelineSkyBox(),
	mDepth(depth_test, depth_write, GL_LEQUAL)
{
}

LLGLSPipelineBlendSkyBox::LLGLSPipelineBlendSkyBox(GLboolean depth_test,
												   GLboolean depth_write)
:	LLGLSPipelineDepthTestSkyBox(depth_test, depth_write),
	mBlend(GL_BLEND)
{
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

