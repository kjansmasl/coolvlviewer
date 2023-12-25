/**
 * @file llfeaturemanager.cpp
 * @brief LLFeatureManager class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <regex>

#include "boost/tokenizer.hpp"

#include "llfeaturemanager.h"

#if LL_WINDOWS
#include "lldxhardware.h"
#endif

#include "lldir.h"
#include "llgl.h"
#include "llnotifications.h"
#include "llrender.h"
#include "llsys.h"
#include "llui.h"
#include "llwindow.h"

#include "llappviewer.h"
#include "lldrawpoolterrain.h"
#include "llgridmanager.h"
#include "llpipeline.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llweb.h"
#include "llworld.h"

LLFeatureManager gFeatureManager;

///////////////////////////////////////////////////////////////////////////////
// LLFeatureInfo class
///////////////////////////////////////////////////////////////////////////////

LLFeatureInfo::LLFeatureInfo(const std::string& name, bool avail, F32 level)
:	mValid(true),
	mName(name),
	mAvailable(avail),
	mRecommendedLevel(level)
{
}

///////////////////////////////////////////////////////////////////////////////
// LLFeatureList class
///////////////////////////////////////////////////////////////////////////////

LLFeatureList::LLFeatureList(const std::string& name)
:	mName(name)
{
}

void LLFeatureList::addFeature(const std::string& name, bool avail, F32 level)
{
	LLFeatureInfo fi(name, avail, level);
	feature_map_t::iterator it = mFeatures.find(name);
	if (it == mFeatures.end())
	{
		mFeatures[name] = fi;
	}
	else
	{
		llwarns << "Attempting to add preexisting feature " << name << llendl;
		it->second = fi;
	}
}

bool LLFeatureList::isFeatureAvailable(const std::string& name)
{
	feature_map_t::iterator it = mFeatures.find(name);
	if (it != mFeatures.end())
	{
		return (it->second).mAvailable;
	}

	llwarns << "Feature " << name << " not in feature list !" << llendl;

	// true so that you have to explicitly disable something for it to be
	// disabled
	return true;
}

// Looks up the specified feature mask and overlay it on top of the current
// feature mask.
void LLFeatureList::maskList(LLFeatureList& mask)
{
	LLFeatureInfo mask_fi;

	feature_map_t::iterator feature_it;
	for (feature_it = mask.mFeatures.begin();
		 feature_it != mask.mFeatures.end(); ++feature_it)
	{
		mask_fi = feature_it->second;
		//
		// Look for the corresponding feature
		//
		feature_map_t::iterator iter = mFeatures.find(mask_fi.mName);
		if (iter == mFeatures.end())
		{
			llwarns << "Feature " << mask_fi.mName
					<< " in mask not in top level !" << llendl;
			continue;
		}

		LLFeatureInfo& cur_fi = iter->second;
		if (mask_fi.mAvailable && !cur_fi.mAvailable)
		{
			llwarns << "Mask attempting to reenabling disabled feature, ignoring "
					<< cur_fi.mName << llendl;
			continue;
		}

		cur_fi.mAvailable = mask_fi.mAvailable;
		cur_fi.mRecommendedLevel = llmin(cur_fi.mRecommendedLevel,
										 mask_fi.mRecommendedLevel);
		LL_DEBUGS("RenderInit") << "Feature mask " << mask.mName
								<< " Feature " << mask_fi.mName
								<< " Mask: " << mask_fi.mRecommendedLevel
								<< " Now: " << cur_fi.mRecommendedLevel
								<< LL_ENDL;
	}

	LL_DEBUGS("RenderInit") << "After applying mask " << mask.mName
							<< std::endl;
		// Will conditionally call dump only if the above message will be
		// logged, thanks to it being wrapped by the LL_DEBUGS and LL_ENDL
		// macros.
		dump();
	LL_CONT << LL_ENDL;
}

void LLFeatureList::dump()
{
	LL_DEBUGS("RenderInit") << "Feature list: " << mName << LL_ENDL;
	LL_DEBUGS("RenderInit") << "------------" << LL_ENDL;

	LLFeatureInfo fi;
	feature_map_t::iterator feature_it;
	for (feature_it = mFeatures.begin(); feature_it != mFeatures.end();
		 ++feature_it)
	{
		fi = feature_it->second;
		LL_DEBUGS("RenderInit") << fi.mName << "\t\t" << fi.mAvailable
								<< ":" << fi.mRecommendedLevel << LL_ENDL;
	}
	LL_DEBUGS("RenderInit") << LL_ENDL;
}

///////////////////////////////////////////////////////////////////////////////
// LLFeatureManager class proper
///////////////////////////////////////////////////////////////////////////////

LLFeatureList* LLFeatureManager::findMask(const std::string& name)
{
	if (mMaskList.count(name))
	{
		return mMaskList[name];
	}

	return NULL;
}

bool LLFeatureManager::maskFeatures(const std::string& name)
{
	LLFeatureList* maskp = findMask(name);
	if (!maskp)
	{
 		LL_DEBUGS("RenderInit") << "Unknown feature mask " << name << LL_ENDL;
		return false;
	}

	LL_DEBUGS("RenderInit") << "Applying Feature Mask: " << name << LL_ENDL;
	maskList(*maskp);
	return true;
}

bool LLFeatureManager::loadFeatureTables()
{
	// *TODO: if to add something else to the skipped list, make this data
	// driven. Put it in the feature table and parse it correctly
	mSkippedFeatures.emplace("RenderAnisotropic");

	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														   "featuretable.txt");
	LL_DEBUGS("RenderInit") << "Looking for feature table in " << filepath
							<< LL_ENDL;

	llifstream file(filepath.c_str());
	if (!file.is_open())
	{
		llwarns << "Unable to open feature table !" << llendl;
		return false;
	}

	std::string name;
	U32 version;
	// Check file version
	file >> name;
	file >> version;
	if (name != "version")
	{
		llwarns << filepath << " does not appear to be a valid feature table !"
				<< llendl;
		file.close();
		return false;
	}

	mTableVersion = version;

	LLFeatureList* flp = NULL;
	while (file >> name)
	{
		char buffer[MAX_STRING];

		if (name.substr(0, 2) == "//")
		{
			// This is a comment.
			file.getline(buffer, MAX_STRING);
			continue;
		}

		if (name.empty())
		{
			// This is a blank line
			file.getline(buffer, MAX_STRING);
			continue;
		}

		if (name == "list")
		{
			// It is a new mask, create it.
			file >> name;
			if (mMaskList.count(name))
			{
				llerrs << "Overriding mask " << name << ", this is invalid !"
					   << llendl;
			}

			flp = new LLFeatureList(name);
			mMaskList[name] = flp;
		}
		else if (!flp)
		{
			llerrs << "Specified parameter before <list> keyword !" << llendl;
		}
		else
		{
			S32 available;
			F32 recommended;
			file >> available >> recommended;
			flp->addFeature(name, available, recommended);
		}
	}
	file.close();

	return true;
}

// This helper class is used to ensure that each generateTextures() call is
// matched by a corresponding deleteTextures() call. It also handles the
// bindManual() calls using those textures.
class LLTextureHolder
{
public:
	LLTextureHolder(U32 unit, U32 size)
	:	mTexUnit(gGL.getTexUnit(unit)),
		mSource(size)					// Pre-allocate vector
	{
		LLImageGL::generateTextures(mSource.size(), &mSource[0]);
	}

	~LLTextureHolder()
	{
		// Ensure that we unbind and delete these textures regardless of how we
		// exit
		if (mTexUnit)
		{
			mTexUnit->unbind(LLTexUnit::TT_TEXTURE);
		}
		LLImageGL::deleteTextures(mSource.size(), &mSource[0]);
	}

	bool bind(U32 index)
	{
		return mTexUnit &&
			   mTexUnit->bindManual(LLTexUnit::TT_TEXTURE, mSource[index]);
	}

private:
	LLTexUnit*			mTexUnit;
	std::vector<U32>	mSource;
};

#if LL_LINUX
// *HACK: to avoid a black (SDL1) or white (SDL2) screen after benchmark, when
// not yet logged in... *TODO: find the root cause for that empty screen, and a
// better way than this dirty trick to fix it... HB
class LLScreenRestorerHelper
{
public:
	LLScreenRestorerHelper() = default;

	~LLScreenRestorerHelper()
	{
		if (gWindowp)
		{
			// This triggers a proper screen refresh (that I sadly could not
			// achieve otherwise), by triggering a full redraw event at the SDL
			// level (redrawing at the viewer level is not enough; I also tried
			// swapping SDL GL buffers before and after benchmark, but to no
			// avail). HB
			gWindowp->refresh();
		}
	}
};
#endif

//static
F32 LLFeatureManager::benchmarkGPU()
{
	if (!gGLManager.mHasTimerQuery)
	{
		// Do not bother benchmarking the fixed function or venerable drivers
		// which do not support accurate timing anyway and are likely to be
		// correctly identified by the GPU table already.
		return -1.f;
	}

	if (!gBenchmarkProgram.mProgramObject)
	{
		// Do not try and benchmark before the shaders get properly
		// initialized, which can only happen *after* the feature manager
		// (which calls this benchmarking routine) has been initialized !
		// The chicken or the egg dilemna... HB
		return -1.f;
	}

#if LL_LINUX
	// *HACK (see above). HB
	LLScreenRestorerHelper restore_on_exit;
#endif

	LLGLDisable blend(GL_BLEND);

	// Measure memory bandwidth by:
	// - allocating a batch of textures and render targets
	// - rendering those textures to those render targets
	// - recording time taken
	// - taking the median time for a given number of samples

	// Resolution of textures/render targets
	constexpr U32 res = 1024;

	// Number of textures
	constexpr U32 count = 32;

	// Number of samples to take
	constexpr S32 samples = 64;

	std::vector<LLRenderTarget> dest(count);
	LLTextureHolder tex_holder(0, count);
	std::vector<F32> results;

	// Build a random texture
	U32 bytes = res * res * 4;
	U8* pixels = new U8[bytes];
	for (U32 i = 0; i < bytes; ++i)
	{
		pixels[i] = (U8)ll_rand(255);
	}

	gGL.setColorMask(true, true);
	LLGLDepthTest depth(GL_FALSE);

	for (U32 i = 0; i < count; ++i)
	{
		// Allocate render targets and textures
		bool success;
		if (gUsePBRShaders)
		{
			success = dest[i].allocate(res, res, GL_RGBA);
		}
		else
		{
			success = dest[i].allocate(res, res, GL_RGBA, false, false);
		}
		if (!success)
		{
			LLMemory::allocationFailed();
			llwarns << "Failed to allocate render target " << i << llendl;
			delete[] pixels;
			return -1.f;
		}

		dest[i].bindTarget();
		dest[i].clear();
		dest[i].flush();

		if (!tex_holder.bind(i))
		{
			llwarns << "Failed to bind tex unit " << i << llendl;
			delete[] pixels;
			return -1.f;
		}
		LLImageGL::setManualImage(GL_TEXTURE_2D, 0, GL_RGBA, res, res, GL_RGBA,
								  GL_UNSIGNED_BYTE, pixels);
	}

	delete[] pixels;

	// Make a dummy triangle to draw with
	LLPointer<LLVertexBuffer> buff =
		new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
	if (!buff->allocateBuffer(3, 0))
	{
		LLMemory::allocationFailed();
		llwarns << "Failed to allocate vertex buffer" << llendl;
		return -1.f;
	}

	LLStrider<LLVector3> v;
	if (!buff->getVertexStrider(v))
	{
		llwarns << "Could not allocate vertex buffer. Benchmark aborted."
				<< llendl;
		return -1.f;
	}

	// Generate a dummy triangle
	v[0].set(-1.f, 1.f, 0.f);
	v[1].set(-1.f, -3.f, 0.f);
	v[2].set(3.f, 1.f, 0.f);

	LLGLSLShader::initProfile();

	buff->unmapBuffer();
	gBenchmarkProgram.bind();

#if 0	// Do not do that: this causes a blank screen under core GL profile.
		// We *already* have a vertex array anyway, when under core GL profile
		// (see mDummyVAO in indra/llrender/llrender.cpp). HB
# ifdef GL_ARB_vertex_array_object
	U32 glarray = 0;
	if (LLRender::sGLCoreProfile)
	{
		glGenVertexArrays(1, &glarray);
		glBindVertexArray(glarray);
	}
# endif
#endif

	buff->setBuffer(LLVertexBuffer::MAP_VERTEX);

	// Wait for any previous GL commands to finish
	glFinish();

	LLTimer timer;
	for (S32 c = -1; c < samples; ++c)
	{
		timer.start();

		for (U32 i = 0; i < count; ++i)
		{
			dest[i].bindTarget();
			tex_holder.bind(i);
			buff->drawArrays(LLRender::TRIANGLES, 0, 3);
			dest[i].flush();
		}

		// Wait for current batch of copies to finish
		glFinish();

		F32 time = timer.getElapsedTimeF32();

		if (c >= 0) // Ignore 1st sample as it tends to be artificially slow
		{
			// Store result in gigabytes per second
			F32 gb = (F32)((F64)(res * res * 8 * count)) / 1000000000.f;
			F32 gbps = gb / time;
			results.push_back(gbps);
		}
	}

#if 0	// Do not do that: this causes a blank screen under core GL profile
# ifdef GL_ARB_vertex_array_object
	if (LLRender::sGLCoreProfile)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &glarray);
	}
# endif
#endif

	LLGLSLShader::finishProfile(false);

	std::sort(results.begin(), results.end());

	F32 gbps = results[results.size() / 2];
	llinfos << "Memory bandwidth is " << llformat("%.3f", gbps)
			<< "GB/s according to CPU timers" << llendl;
#if LL_DARWIN
	if (gbps > 512.f)
	{
		llinfos << "Memory bandwidth is improbably high and likely incorrect."
				<< llendl;
		// OS-X is probably lying, discard result
		return -1.f;
	}
#endif

	F32 ms = gBenchmarkProgram.mTimeElapsed / 1000000.f;
	F32 seconds = ms / 1000.f;

	F64 samples_drawn = res * res * count * samples;
	F32 samples_sec = (F32)(samples_drawn / 1000000000.0) / seconds;
	gbps = samples_sec * 8;

	llinfos << "Memory bandwidth is " << llformat("%.3f", gbps)
			<< "GB/s according to ARB_timer_query" << llendl;

	gBenchmarkProgram.unbind();

	return gbps;
}

void LLFeatureManager::loadGPUClass(bool benchmark_gpu)
{
	LL_DEBUGS("RenderInit") << "Loading GPU class..." << LL_ENDL;

	// Defaults
	mGPUMemoryBandwidth = 0.f;		// 0 = not benchmarked
	mGPUSupported = false;
	mGPUClass = GPU_CLASS_UNKNOWN;
	const std::string raw_renderer = gGLManager.getRawGLString();
	mGPUString = raw_renderer;

	if (benchmark_gpu)
	{
		// Bias GPU performances with CPU speed
		F32 class0_gbps = llmax(gSavedSettings.getU32("GPUMemoryBWClassBase"),
								1U);
		// This factor will be 1.f for a CPU core CPUBenchmarkPerfFactor times
		// weaker than the core of a 9700K @ 5GHz. HB
		F32 cpu_bias = llclamp(gSavedSettings.getF32("CPUBenchmarkPerfFactor"),
							   0.1f, 10.f);
		// Multiply our adjustment factor by actual CPU core benchmark factor
		cpu_bias *= LLCPUInfo::getInstance()->benchmarkFactor();
		// Get GPU memory bandwidth from benchmark
		mGPUMemoryBandwidth = benchmarkGPU();
		F32 gbps = mGPUMemoryBandwidth * cpu_bias;
		if (gbps < 0.f)
		{
			// Could not bench, use GLVersion
#if LL_DARWIN
			// GLVersion is misleading on macOS, just default to class 2 if we
			// cannot bench
			mGPUClass = GPU_CLASS_2;
#else
			if (gGLManager.mGLVersion <= 2.f)
			{
				mGPUClass = GPU_CLASS_0;
			}
			else if (gGLManager.mGLVersion <= 3.f)
			{
				mGPUClass = GPU_CLASS_1;
			}
			else if (gGLManager.mGLVersion < 3.3f)
			{
				mGPUClass = GPU_CLASS_2;
			}
			else if (gGLManager.mGLVersion < 4.f)
			{
				mGPUClass = GPU_CLASS_3;
			}
			else if (gGLManager.mGLVersion < 4.4f)
			{
				mGPUClass = GPU_CLASS_4;
			}
			else
			{
				mGPUClass = GPU_CLASS_5;
			}
			if (gGLManager.mIsIntel && mGPUClass > GPU_CLASS_1)
			{
				// Intel GPUs are generally weaker than other GPUs despite
				// having advanced OpenGL features
				mGPUClass = (EGPUClass)(mGPUClass - 1);
			}
#endif
		}
		else if (gbps < class0_gbps || gGLManager.mGLVersion <= 2.f)
		{
			mGPUClass = GPU_CLASS_0;
		}
		else if (gbps < 2.f * class0_gbps || gGLManager.mGLVersion <= 3.f)
		{
			mGPUClass = GPU_CLASS_1;
		}
		else if (gbps < 4.f * class0_gbps || gGLManager.mGLVersion < 3.3f)
		{
			mGPUClass = GPU_CLASS_2;
		}
		else if (gbps < 8.f * class0_gbps || gGLManager.mGLVersion < 4.f)
		{
			mGPUClass = GPU_CLASS_3;
		}
		else if (gbps < 16.f * class0_gbps || gGLManager.mGLVersion < 4.4f)
		{
			mGPUClass = GPU_CLASS_4;
		}
		else
		{
			mGPUClass = GPU_CLASS_5;
		}

		mGPUSupported = mGPUClass > GPU_CLASS_0;
		if (mGPUSupported)
		{
			std::string msg =
				llformat("GPU is considered supported (class %d). Class deduced from ",
						 (S32)mGPUClass);
			if (gbps > 0)
			{
				
				llinfos << msg
						<< "CPU speed-pondered GPU memory benchmark: "
						<< (S32)gbps << "GB/s" << llendl;
			}
			else
			{
				llinfos << msg << "advertized OpenGL version: "
						<< gGLManager.mGLVersion << llendl;
			}
			gSavedSettings.setS32("LastGPUClass", mGPUClass);
			gSavedSettings.setString("LastGPUString", raw_renderer);
		}
		else
		{
			llwarns << "GPU is not supported !" << llendl;
		}
		return;
	}

	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														  "gpu_table.txt");
	llifstream file(filepath.c_str());
	if (!file.is_open())
	{
		llwarns << "Unable to open GPU table: " << filepath
				<< ". Using the GPU benchmarking method instead..." << llendl;
		loadGPUClass(true);	// Retry with benchmarking
		return;
	}

	std::string renderer = raw_renderer;
	for (std::string::iterator i = renderer.begin(); i != renderer.end();
		 ++i)
	{
		*i = tolower(*i);
	}

	bool found = false;

	for (U32 line = 0; !found && !file.eof(); ++line)
	{
		char buffer[MAX_STRING];
		buffer[0] = 0;

		file.getline(buffer, MAX_STRING);

		if (strlen(buffer) == 0)
		{
			// This is a blank line
			continue;
		}
		if (strlen(buffer) >= 2 && buffer[0] == '/' && buffer[1] == '/')
		{
			// This is a comment.
			continue;
		}

		// Setup the tokenizer
		std::string buf(buffer);
		std::string cls, label, expr, supported;
		typedef boost::tokenizer<boost::char_separator<char> > tok;
		tok tokens(buf, boost::char_separator<char>("\t\n"));
		tok::iterator token_iter = tokens.begin();

		// Grab the label, pseudo regular expression, and class
		if (token_iter != tokens.end())
		{
			label = *token_iter++;
		}
		if (token_iter != tokens.end())
		{
			expr = *token_iter++;
		}
		if (token_iter != tokens.end())
		{
			cls = *token_iter++;
		}
		if (token_iter != tokens.end())
		{
			supported = *token_iter++;
		}

		if (label.empty() || expr.empty() || cls.empty() || supported.empty())
		{
			llwarns << "Invald gpu_table.txt at line " << line << ": '"
					<< buffer << "'" << llendl;
			continue;
		}

		for (U32 i = 0; i < expr.length(); ++i)
		{
			expr[i] = tolower(expr[i]);
		}

		bool result = false;
		try
		{
			// Run the regular expression against the renderer
			std::regex re(expr.c_str());
			result = std::regex_search(renderer, re);
		}
		catch (std::regex_error& e)
		{
			llwarns << "Regex error: " << e.what() << " at line " << line
					<< llendl;
			continue;
		}
		if (result)
		{
			// If we found it, stop !
			found = true;
			mGPUString = label;
			mGPUClass = (EGPUClass)strtol(cls.c_str(), NULL, 10);
			mGPUSupported = strtol(supported.c_str(), NULL, 10);
			break;
		}
	}

	file.close();

	std::string last_seen = gSavedSettings.getString("LastGPUString");
	if (raw_renderer != last_seen)
	{
		// Enable core GL profile for recognized NVIDIA cards when not yet
		// seen in former sessions (i.e. we preserve the user choice after
		// the first session). HB
		if (gGLManager.mIsNVIDIA && gGLManager.mGLVersion >= 3.f &&
			!gSavedSettings.getBool("RenderGLCoreProfile"))
		{		
			gSavedSettings.setBool("RenderGLCoreProfile", true);
		}
	}

	if (!found)
	{
		// Do not re-benchmark at each sessions an already benchmarked
		// "unknown" GPU; simply reuse the stored GPU class as computed
		// on first launch with this GPU. HB
		if (last_seen == raw_renderer)
		{
			mGPUClass = (EGPUClass)gSavedSettings.getS32("LastGPUClass");
			mGPUClass = llclamp(mGPUClass, GPU_CLASS_UNKNOWN, GPU_CLASS_5);
			mGPUSupported = mGPUClass != GPU_CLASS_UNKNOWN;
		}
		if (mGPUSupported)
		{
			llinfos << "GPU '" << raw_renderer
					<< "' already benchmarked and deemed compatible."
					<< llendl;
		}
		else
		{
			llinfos << "GPU '" << raw_renderer
					<< "' not recognized, using the GPU benchmarking method instead..."
					<< llendl;
			loadGPUClass(true);	// Retry with benchmarking
			return;
		}
	}

	if (mGPUSupported)
	{
		if (found)
		{
			llinfos << "GPU '" << raw_renderer << "' recognized as '"
					<< mGPUString << "' and is supported." << llendl;
		}
		gSavedSettings.setS32("LastGPUClass", mGPUClass);
		gSavedSettings.setString("LastGPUString", raw_renderer);
	}
	else
	{
		llwarns << "GPU '" << raw_renderer << "' recognized as '"
				<< mGPUString << "' and is not supported !" << llendl;
	}
}

void LLFeatureManager::cleanupFeatureTables()
{
	std::for_each(mMaskList.begin(), mMaskList.end(), DeletePairedPointer());
	mMaskList.clear();
}

void LLFeatureManager::init()
{
	// Load the tables
	loadFeatureTables();

	// Get the GPU class from gpu_table.txt (or from the OpenGL version). We
	// cannot benchmark this early in the viewer initialization processs
	// because the shaders have not yet been loaded. HB
	loadGPUClass(false);

	// Apply the base masks, so we know if anything is disabled
	applyBaseMasks();
}

void LLFeatureManager::applyRecommendedSettings()
{
	llinfos << "Applying recommended features." << llendl;
	loadGPUClass(gSavedSettings.getBool("BenchmarkGPU"));
	// Do not go too far on recommended level: level 4 max (no shadows)
	S32 level = llmin(mGPUClass, GPU_CLASS_4);
	setGraphicsLevel(level, false);
	gSavedSettings.setU32("RenderQualityPerformance", level);
	// Enable core GL profile for NVIDIA cards with OpenGL v3+. HB
	if (gGLManager.mIsNVIDIA && gGLManager.mGLVersion >= 3.f &&
		!gSavedSettings.getBool("RenderGLCoreProfile"))
	{		
		gSavedSettings.setBool("RenderGLCoreProfile", true);
		gNotifications.add("CoreProfileAfterRestart");
	}
}

// See featuretable.txt / featuretable_linux.txt / featuretable_mac.txt
void LLFeatureManager::applyFeatures(bool skip_features)
{
#if LL_DEBUG
	dump();
#endif

	// Scroll through all of these and set their corresponding control value
	for (feature_map_t::iterator mIt = mFeatures.begin();
		 mIt != mFeatures.end(); ++mIt)
	{
		const std::string& name = mIt->first;

		// Skip features you want to skip; do this for when you do not want to
		// change certain settings
		if (skip_features && mSkippedFeatures.count(name))
		{
			continue;
		}

		// Get the control setting
		LLControlVariable* ctrl = gSavedSettings.getControl(name.c_str());
		if (!ctrl)
		{
			llwarns << "Control setting " << name << " does not exist !"
					<< llendl;
			continue;
		}

		F32 recommended = 0.f;
		if (mIt->second.mAvailable)
		{
			recommended = mIt->second.mRecommendedLevel;
		}
		else
		{
			llwarns << "Feature " << name << " not available !" << llendl;
		}

		// Handle all the different types
		if (ctrl->isType(TYPE_BOOLEAN))
		{
			gSavedSettings.setBool(name.c_str(), recommended != 0.f);
		}
		else if (ctrl->isType(TYPE_S32))
		{
			gSavedSettings.setS32(name.c_str(), (S32)recommended);
		}
		else if (ctrl->isType(TYPE_U32))
		{
			gSavedSettings.setU32(name.c_str(), (U32)recommended);
		}
		else if (ctrl->isType(TYPE_F32))
		{
			gSavedSettings.setF32(name.c_str(), recommended);
		}
		else
		{
			llwarns << "Control variable is not a numeric type !" << llendl;
		}
	}
}

void LLFeatureManager::setGraphicsLevel(S32 level, bool skip_features)
{
	LLViewerShaderMgr::sSkipReload = true;
	clear_glerror();
	applyBaseMasks();

	switch (level)
	{
		case 0:
		default:
			maskFeatures("Low");
			break;

		case 1:
			maskFeatures("Mid1");
			break;

		case 2:
			maskFeatures("Mid2");
			break;

		case 3:
			maskFeatures("High1");
			break;

		case 4:
			maskFeatures("High2");
			break;

		case 5:
			maskFeatures("Ultra");
	}

	applyFeatures(skip_features);

	LLViewerShaderMgr::sSkipReload = false;
	gViewerShaderMgrp->setShaders();
}

void LLFeatureManager::applyBaseMasks()
{
	// Re-apply masks
	mFeatures.clear();

	LLFeatureList* maskp = findMask("all");
	if (!maskp)
	{
		llwarns << "Missing \"all\" list in feature table !" << llendl;
		return;
	}

	mFeatures = maskp->getFeatures();

	// Mask class
	if ((S32)mGPUClass >= 0 && (S32)mGPUClass < 6)
	{
		static const char* class_table[] =
		{
			"Class0",
			"Class1",
			"Class2",
			"Class3",
			"Class4",
			"Class5"
		};

		llinfos << "Setting GPU class to: " << class_table[mGPUClass] << llendl;
		maskFeatures(class_table[mGPUClass]);
	}
	else
	{
		llinfos << "Setting GPU class to: Unknown" << llendl;
		maskFeatures("Unknown");
	}

	// Now all those wacky ones
	if (gGLManager.mIsNVIDIA)
	{
		maskFeatures("NVIDIA");
	}
	if (gGLManager.mIsAMD)
	{
		maskFeatures("ATI");
	}
	if (gGLManager.mIsIntel)
	{
		maskFeatures("Intel");
	}
	if (gGLManager.mGLVersion < 3.f)
	{
		maskFeatures("OpenGLPre30");
	}
	if (gGLManager.mGLVersion < 4.f)
	{
		maskFeatures("OpenGLPre40");
	}
	if (gGLManager.mVRAM > 512)
	{
		maskFeatures("VRAMGT512");
	}

	// Now mask by gpu string. Replaces ' ' with '_' in mGPUString to deal with
	// inability for parser to handle spaces.
	std::string gpustr = mGPUString;
	for (std::string::iterator iter = gpustr.begin(); iter != gpustr.end();
		 ++iter)
	{
		if (*iter == ' ')
		{
			*iter = '_';
		}
	}

	LL_DEBUGS("RenderInit") << "Masking features from GPU table match: "
							<< gpustr << LL_ENDL;
	maskFeatures(gpustr);

	if (isSafe())
	{
		maskFeatures("safe");
	}
}
