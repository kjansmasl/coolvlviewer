/**
 * @file llmaterialmgr.cpp
 * @brief Material manager
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include <utility>

#include "llmaterialmgr.h"

#include "llcallbacklist.h"
#include "llcorehttpcommon.h"
#include "llcorehttputil.h"
#include "llfasttimer.h"
#include "llhttpsdhandler.h"
#include "llsdserialize.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llworld.h"

// Materials capability parameters
#define MATERIALS_CAPABILITY_NAME			"RenderMaterials"
#define MATERIALS_CAP_ZIP_FIELD				"Zipped"
#define MATERIALS_CAP_FULL_PER_FACE_FIELD	"FullMaterialsPerFace"
#define MATERIALS_CAP_FACE_FIELD			"Face"
#define MATERIALS_CAP_MATERIAL_FIELD		"Material"
#define MATERIALS_CAP_OBJECT_ID_FIELD		"ID"
#define MATERIALS_CAP_MATERIAL_ID_FIELD		"MaterialID"

// Network timeouts
#define MATERIALS_GET_TIMEOUT	120.f
#define MATERIALS_POST_TIMEOUT	120.f

class LLMaterialHttpHandler final : public LLHttpSDHandler
{
protected:
	LOG_CLASS(LLMaterialHttpHandler);

public: 
	typedef boost::function<void(bool, const LLSD&)> CallbackFunction;
	typedef std::shared_ptr<LLMaterialHttpHandler> ptr_t;

	LLMaterialHttpHandler(const std::string& method, CallbackFunction cback)
	:	LLHttpSDHandler(),
		mMethod(method),
		mCallback(cback)
	{
	}

	~LLMaterialHttpHandler() override
	{
	}

protected:
	void onSuccess(LLCore::HttpResponse*, const LLSD& content) override
	{
		if (mCallback)
		{
			mCallback(true, content);
		}
	}

	void onFailure(LLCore::HttpResponse* response,
				   LLCore::HttpStatus status) override
	{
		if (response)
		{
			llwarns << mMethod << " Error: " << status.toULong()
					<< " - Cannot access capability: "
					<< MATERIALS_CAPABILITY_NAME << " - with URL: "
					<< response->getRequestURL() << " - reason: "
					<< status.toString() << llendl;
		}
		mCallback(false, LLSD());
	}

private:
	std::string			mMethod;
	CallbackFunction	mCallback;
};

LLMaterialMgr::LLMaterialMgr()
{
	LLAppCoreHttp& app_core_http(gAppViewerp->getAppCoreHttp());
	mHttpPolicy = app_core_http.getPolicy(LLAppCoreHttp::AP_MATERIALS);
	mHttpRequest = DEFAULT_HTTP_REQUEST;
	mHttpHeaders = DEFAULT_HTTP_HEADERS;
	mHttpOptions = DEFAULT_HTTP_OPTIONS;
	mHttpAdapter =
		std::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>("processGetAllQueue");
	mMaterials.emplace(LLMaterialID::null, LLMaterialPtr(NULL));
	gIdleCallbacks.addFunction(&LLMaterialMgr::onIdle, NULL);
	gWorld.setRegionRemovedCallback(boost::bind(&LLMaterialMgr::onRegionRemoved,
												this, _1));
}

LLMaterialMgr::~LLMaterialMgr()
{
	gIdleCallbacks.deleteFunction(&LLMaterialMgr::onIdle, NULL);
	mHttpAdapter.reset();
	mHttpRequest.reset();
	mHttpOptions.reset();
	mHttpHeaders.reset();
	llinfos << "Number of stored materials: " << mMaterials.size() << llendl;
}

bool LLMaterialMgr::isGetPending(const LLUUID& region_id,
								 const LLMaterialID& material_id) const
{
	get_pending_map_t::const_iterator it =
		mGetPending.find(RegionMaterialPair(region_id, material_id));
	return it != mGetPending.end() &&
		   LLFrameTimer::getTotalSeconds() < it->second + MATERIALS_POST_TIMEOUT;
}

void LLMaterialMgr::markGetPending(const LLUUID& region_id,
								   const LLMaterialID& material_id)
{
	get_pending_map_t::iterator it =
		mGetPending.find(RegionMaterialPair(region_id, material_id));
	if (it == mGetPending.end())
	{
		mGetPending.emplace(RegionMaterialPair(region_id, material_id),
							LLFrameTimer::getTotalSeconds());
	}
	else
	{
		it->second = LLFrameTimer::getTotalSeconds();
	}
}

const LLMaterialPtr LLMaterialMgr::get(const LLUUID& region_id,
									   const LLMaterialID& material_id)
{
	LL_DEBUGS("Materials") << "region: " << region_id << " - material id: "
						   << material_id << LL_ENDL;

	material_map_t::const_iterator mit = mMaterials.find(material_id);
	if (mit != mMaterials.end())
	{
		LL_DEBUGS("Materials") << "Found material " << material_id << LL_ENDL;
		return mit->second;
	}

	if (!isGetPending(region_id, material_id))
	{
		LL_DEBUGS("Materials") << "Material pending: " << material_id
							   << LL_ENDL;
		get_queue_t::iterator qit = mGetQueue.find(region_id);
		if (qit == mGetQueue.end())
		{
			LL_DEBUGS("Materials") << "mGetQueue add region: " << region_id
								   << " - pending material: " << material_id
								   << LL_ENDL;
			std::pair<get_queue_t::iterator, bool> ret =
				mGetQueue.emplace(region_id, material_queue_t());
			qit = ret.first;
		}
		qit->second.emplace(material_id);
		markGetPending(region_id, material_id);
	}

	LL_DEBUGS("Materials") << "Returning empty material " << LL_ENDL;
	return LLMaterialPtr();
}

boost::signals2::connection LLMaterialMgr::get(const LLUUID& region_id,
											   const LLMaterialID& material_id,
											   LLMaterialMgr::get_callback_t::slot_type cb)
{
	boost::signals2::connection connection;

	material_map_t::const_iterator mit = mMaterials.find(material_id);
	if (mit != mMaterials.end())
	{
		LL_DEBUGS("Materials") << "Region " << region_id
							   << ", found materialid " << material_id
							   << LL_ENDL;
		get_callback_t signal;
		signal.connect(cb);
		signal(material_id, mit->second);
		connection = boost::signals2::connection();
	}
	else
	{
		if (!isGetPending(region_id, material_id))
		{
			get_queue_t::iterator qit = mGetQueue.find(region_id);
			if (qit == mGetQueue.end())
			{
				LL_DEBUGS("Materials") << "mGetQueue inserting region: "
									   << region_id << LL_ENDL;
				std::pair<get_queue_t::iterator, bool> ret =
					mGetQueue.emplace(region_id, material_queue_t());
				qit = ret.first;
			}
			LL_DEBUGS("Materials") << "adding material id " << material_id << LL_ENDL;
			qit->second.emplace(material_id);
			markGetPending(region_id, material_id);
		}

		get_callback_map_t::iterator cb_it = mGetCallbacks.find(material_id);
		if (cb_it == mGetCallbacks.end())
		{
			std::pair<get_callback_map_t::iterator, bool> ret =
				mGetCallbacks.emplace(material_id, new get_callback_t());
			cb_it = ret.first;
		}
		connection = cb_it->second->connect(cb);
	}

	return connection;
}

boost::signals2::connection LLMaterialMgr::getTE(const LLUUID& region_id,
												 const LLMaterialID& material_id,
												 U32 te,
												 LLMaterialMgr::get_callback_te_t::slot_type cb)
{
	boost::signals2::connection connection;

	material_map_t::const_iterator mit = mMaterials.find(material_id);
	if (mit != mMaterials.end())
	{
		LL_DEBUGS("Materials") << "Region: " << region_id
							   << " - Found materialid: " << material_id
							   << LL_ENDL;
		get_callback_te_t signal;
		signal.connect(cb);
		signal(material_id, mit->second, te);
		connection = boost::signals2::connection();
	}
	else
	{
		if (!isGetPending(region_id, material_id))
		{
			get_queue_t::iterator qit = mGetQueue.find(region_id);
			if (mGetQueue.end() == qit)
			{
				LL_DEBUGS("Materials") << "mGetQueue inserting region: "
									   << region_id << LL_ENDL;
				std::pair<get_queue_t::iterator, bool> ret =
					mGetQueue.emplace(region_id, material_queue_t());
				qit = ret.first;
			}
			LL_DEBUGS("Materials") << "Adding material id: " << material_id
								   << LL_ENDL;
			qit->second.emplace(material_id);
			markGetPending(region_id, material_id);
		}

		TEMaterialPair te_mat_pair;
		te_mat_pair.mTE = te;
		te_mat_pair.mMaterialId = material_id;

		get_callback_te_map_t::iterator cb_it = mGetTECallbacks.find(te_mat_pair);
		if (cb_it == mGetTECallbacks.end())
		{
			std::pair<get_callback_te_map_t::iterator, bool> ret =
				mGetTECallbacks.emplace(te_mat_pair, new get_callback_te_t());
			cb_it = ret.first;
		}
		connection = cb_it->second->connect(cb);
	}

	return connection;
}

bool LLMaterialMgr::isGetAllPending(const LLUUID& region_id) const
{
	getall_pending_map_t::const_iterator it = mGetAllPending.find(region_id);
	return it != mGetAllPending.end() &&
		   LLFrameTimer::getTotalSeconds() < it->second + MATERIALS_GET_TIMEOUT;
}

void LLMaterialMgr::getAll(const LLUUID& region_id)
{
	if (!isGetAllPending(region_id))
	{
		LL_DEBUGS("Materials") << "Queuing for region " << region_id
							   << LL_ENDL;
		mGetAllQueue.emplace(region_id);
	}
	else
	{
		LL_DEBUGS("Materials") << "Already pending for region " << region_id
							   << LL_ENDL;
	}
}

boost::signals2::connection LLMaterialMgr::getAll(const LLUUID& region_id,
												  LLMaterialMgr::getall_callback_t::slot_type cb)
{
	if (!isGetAllPending(region_id))
	{
		mGetAllQueue.emplace(region_id);
	}

	getall_callback_map_t::iterator cb_it = mGetAllCallbacks.find(region_id);
	if (cb_it == mGetAllCallbacks.end())
	{
		std::pair<getall_callback_map_t::iterator, bool> ret =
			mGetAllCallbacks.emplace(region_id, new getall_callback_t());
		cb_it = ret.first;
	}
	return cb_it->second->connect(cb);
}

void LLMaterialMgr::put(const LLUUID& object_id, U8 te, const LLMaterial& mat)
{
	put_queue_t::iterator qit = mPutQueue.find(object_id);
	if (qit == mPutQueue.end())
	{
		LL_DEBUGS("Materials") << "mPutQueue insert object " << object_id
							   << LL_ENDL;
		mPutQueue.emplace(object_id, facematerial_map_t());
		qit = mPutQueue.find(object_id);
	}

	facematerial_map_t::iterator fit = qit->second.find(te);
	if (fit == qit->second.end())
	{
		qit->second.emplace(te, mat);
	}
	else
	{
		fit->second = mat;
	}
}

void LLMaterialMgr::remove(const LLUUID& object_id, U8 te)
{
	put(object_id, te, LLMaterial::null);
}

void LLMaterialMgr::setLocalMaterial(const LLUUID& region_id,
									 LLMaterialPtr material_ptr)
{
	LLUUID uuid;
	uuid.generate();
	LLMaterialID material_id(uuid);
	LL_DEBUGS("Materials") << "Created a new local material: " << material_id
						   << " - region: " << region_id << LL_ENDL;
	mMaterials.emplace(material_id, material_ptr);
	mGetPending.erase(RegionMaterialPair(region_id, material_id));
}

const LLMaterialPtr LLMaterialMgr::setMaterial(const LLUUID& region_id,
											   const LLMaterialID& material_id,
											   const LLSD& material_data)
{
	LL_DEBUGS("Materials") << "Region: " << region_id << " - material id: "
						   << material_id << LL_ENDL;
	material_map_t::const_iterator it = mMaterials.find(material_id);
	if (it == mMaterials.end())
	{
		LL_DEBUGS("Materials") << "New material" << LL_ENDL;
		LLMaterialPtr new_matp(new LLMaterial(material_data));
		it = mMaterials.emplace(material_id, std::move(new_matp)).first;
	}

	setMaterialCallbacks(material_id, it->second);
	mGetPending.erase(RegionMaterialPair(region_id, material_id));

	return it->second;
}

void LLMaterialMgr::setMaterialCallbacks(const LLMaterialID& material_id,
										 const LLMaterialPtr& material_ptr)
{
	TEMaterialPair te_mat_pair;
	te_mat_pair.mMaterialId = material_id;

	U32 i = 0;
	while (i < MAX_TES && !mGetTECallbacks.empty())
	{
		te_mat_pair.mTE = i++;
		get_callback_te_map_t::iterator te_it =
			mGetTECallbacks.find(te_mat_pair);
		if (te_it != mGetTECallbacks.end())
		{
			(*te_it->second)(material_id, material_ptr, te_mat_pair.mTE);
			delete te_it->second;
			mGetTECallbacks.hmap_erase(te_it);
		}
	}

	get_callback_map_t::iterator cb_it = mGetCallbacks.find(material_id);
	if (cb_it != mGetCallbacks.end())
	{
		(*cb_it->second)(material_id, material_ptr);

		delete cb_it->second;
		mGetCallbacks.hmap_erase(cb_it);
	}
}

void LLMaterialMgr::onGetResponse(bool success, const LLSD& content,
								  const LLUUID& region_id)
{
	if (!success)
	{
		// *TODO: is there any kind of error handling we can do here ?
		llwarns << "Failed in region: " << region_id << llendl;
		return;
	}

	if (!content.isMap() || !content.has(MATERIALS_CAP_ZIP_FIELD) ||
		!content[MATERIALS_CAP_ZIP_FIELD].isBinary())
	{
		llwarns << "Invalid response LLSD in region: " << region_id << llendl;
		return;
	}

	const LLSD::Binary& bin_data = content[MATERIALS_CAP_ZIP_FIELD].asBinary();
	LLSD response_data;
	if (!unzip_llsd(response_data, bin_data.data(), bin_data.size()))
	{
		llwarns << "Cannot unzip LLSD binary content in region: " << region_id
				<< llendl;
		return;
	}
	if (!response_data.isArray())
	{
		llwarns << "Invalid response data LLSD in region: " << region_id
				<< llendl;
		return;
	}

	LL_DEBUGS("Materials") << "Response has "<< response_data.size()
						   << " materials" << LL_ENDL;
	for (LLSD::array_const_iterator it = response_data.beginArray(),
									end = response_data.endArray();
		 it != end; ++it)
	{
		const LLSD& material_data = *it;
		if (!material_data.isMap() ||
			!material_data.has(MATERIALS_CAP_OBJECT_ID_FIELD) ||
			!material_data[MATERIALS_CAP_OBJECT_ID_FIELD].isBinary() ||
			!material_data.has(MATERIALS_CAP_MATERIAL_FIELD) ||
			!material_data[MATERIALS_CAP_MATERIAL_FIELD].isMap())
		{
			llwarns << "Invalid material data LLSD in region: " << region_id
					<< llendl;
			continue;
		}

		const LLSD::Binary& bin_data =
			material_data[MATERIALS_CAP_OBJECT_ID_FIELD].asBinary();
		if (bin_data.size() != UUID_BYTES)
		{
			llwarns << "Invalid material Id binary bucket size: "
					<< bin_data.size() << " (should be "
					<< UUID_BYTES << ") - Region: " << region_id
					<< llendl;
			continue;
		}
		LLMaterialID material_id(bin_data);

		setMaterial(region_id, material_id,
					material_data[MATERIALS_CAP_MATERIAL_FIELD]);
	}
}

void LLMaterialMgr::onGetAllResponse(bool success, const LLSD& content,
									 const LLUUID& region_id)
{
	if (!success)
	{
		// *TODO: is there any kind of error handling we can do here?
		llwarns << "Failed in region: " << region_id << llendl;
		return;
	}

	if (!content.isMap() || !content.has(MATERIALS_CAP_ZIP_FIELD) ||
		!content[MATERIALS_CAP_ZIP_FIELD].isBinary())
	{
		llwarns << "Invalid response LLSD in region: " << region_id << llendl;
		return;
	}

	const LLSD::Binary& bin_data = content[MATERIALS_CAP_ZIP_FIELD].asBinary();
	LLSD response_data;
	if (!unzip_llsd(response_data, bin_data.data(), bin_data.size()))
	{
		llwarns << "Cannot unzip LLSD binary content in region: " << region_id
				<< llendl;
		return;
	}
	if (!response_data.isArray())
	{
		llwarns << "Invalid response data LLSD in region: " << region_id
				<< llendl;
		return;
	}

	get_queue_t::iterator qit = mGetQueue.find(region_id);
	material_map_t materials;

	LL_DEBUGS("Materials") << "response has "<< response_data.size()
						   << " materials" << LL_ENDL;
	for (LLSD::array_const_iterator it = response_data.beginArray(),
									end = response_data.endArray();
		 it != end; ++it)
	{
		const LLSD& material_data = *it;
		if (!material_data.isMap() ||
			!material_data.has(MATERIALS_CAP_OBJECT_ID_FIELD) ||
			!material_data[MATERIALS_CAP_OBJECT_ID_FIELD].isBinary())
		{
			llwarns << "Invalid material data LLSD (1) in region: "
					<< region_id << llendl;
			continue;
		}

		const LLSD::Binary& bin_data =
			material_data[MATERIALS_CAP_OBJECT_ID_FIELD].asBinary();
		if (bin_data.size() != UUID_BYTES)
		{
			llwarns << "Invalid material Id binary bucket size: "
					<< bin_data.size() << " (should be "
					<< UUID_BYTES << ") - Region: " << region_id
					<< llendl;
			continue;
		}
		LLMaterialID material_id(bin_data);

		if (mGetQueue.end() != qit)
		{
			qit->second.erase(material_id);
		}

		if (!material_data.has(MATERIALS_CAP_MATERIAL_FIELD) ||
			!material_data[MATERIALS_CAP_MATERIAL_FIELD].isMap())
		{
			llwarns << "Invalid material data LLSD (2) in region: "
					<< region_id << llendl;
			continue;
		}

		LLMaterialPtr material =
			setMaterial(region_id, material_id,
						material_data[MATERIALS_CAP_MATERIAL_FIELD]);

		materials[material_id] = material;
	}

	getall_callback_map_t::iterator cb_it = mGetAllCallbacks.find(region_id);
	if (cb_it != mGetAllCallbacks.end() && cb_it->second)
	{
		(*cb_it->second)(region_id, materials);

		delete cb_it->second;
		mGetAllCallbacks.hmap_erase(cb_it);
	}

	if (qit != mGetQueue.end() && qit->second.empty())
	{
		mGetQueue.hmap_erase(qit);
	}

	LL_DEBUGS("Materials") << "Recording that getAll has been done for region: "
						   << region_id << LL_ENDL;
	// Prevents subsequent getAll requests for this region
	mGetAllRequested.emplace(region_id);
	// Invalidates region_id
	mGetAllPending.erase(region_id);
}

void LLMaterialMgr::onPutResponse(bool success, const LLSD& content)
{
	if (!success)
	{
		// *TODO: is there any kind of error handling we can do here ?
		llwarns << "Failed" << llendl;
		return;
	}

	if (!content.isMap() || !content.has(MATERIALS_CAP_ZIP_FIELD) ||
		!content[MATERIALS_CAP_ZIP_FIELD].isBinary())
	{
		llwarns << "Invalid response LLSD" << llendl;
		return;
	}

	const LLSD::Binary& bin_data = content[MATERIALS_CAP_ZIP_FIELD].asBinary();
	LLSD response_data;
	if (!unzip_llsd(response_data, bin_data.data(), bin_data.size()))
	{
		llwarns << "Cannot unzip LLSD binary content" << llendl;
		return;
	}
	if (response_data.isArray())
	{
		LL_DEBUGS("Materials") << "Response has " << response_data.size()
							   << " materials" << LL_ENDL;
	}
	else
	{
		llwarns << "Invalid response data LLSD" << llendl;
	}
}

void LLMaterialMgr::onIdle(void*)
{
	LL_FAST_TIMER(FTM_MATERIALS_IDLE);

	LLMaterialMgr* self = getInstance();

	if (!self->mGetQueue.empty())
	{
		self->processGetQueue();
	}

	if (!self->mGetAllQueue.empty())
	{
		self->processGetAllQueue();
	}

	if (!self->mPutQueue.empty())
	{
		self->processPutQueue();
	}

	self->mHttpRequest->update(0L);
}

void LLMaterialMgr::processGetQueue()
{
	get_queue_t::iterator loop_rqit = mGetQueue.begin();
	while (loop_rqit != mGetQueue.end())
	{
		get_queue_t::iterator rqit = loop_rqit++;

		LLUUID region_id = rqit->first;
		if (isGetAllPending(region_id))
		{
			continue;
		}

		LLViewerRegion* regionp = gWorld.getRegionFromID(region_id);
		if (!regionp)
		{
			llwarns << "Unknown region with id " << region_id << llendl;
			mGetQueue.hmap_erase(rqit);
			continue;
		}
		else if (!regionp->capabilitiesReceived() ||
				 regionp->materialsCapThrottled())
		{
			continue;
		}
		else if (mGetAllRequested.find(region_id) == mGetAllRequested.end())
		{
			LL_DEBUGS("Materials") << "Calling getAll for "
								   << regionp->getName() << LL_ENDL;
			getAll(region_id);
			continue;
		}

		const std::string cap_url =
			regionp->getCapability(MATERIALS_CAPABILITY_NAME);
		if (cap_url.empty())
		{
			llwarns << "Capability '" << MATERIALS_CAPABILITY_NAME
					<< "' is not defined on region: "
					<< regionp->getIdentity() << llendl;
			mGetQueue.hmap_erase(rqit);
			continue;
		}

		LLSD mats_data = LLSD::emptyArray();

		material_queue_t& materials = rqit->second;
		U32 max_entries = regionp->getMaxMaterialsPerTransaction();
		material_queue_t::iterator loop_mit = materials.begin();
		while (loop_mit != materials.end() &&
			   (U32)mats_data.size() <= max_entries)
		{
			material_queue_t::iterator mit = loop_mit++;
			mats_data.append(mit->asLLSD());
			markGetPending(region_id, *mit);
			materials.erase(mit);
		}
		if (materials.empty())
		{
			mGetQueue.hmap_erase(rqit);
		}

		std::string material_str = zip_llsd(mats_data);
		S32 mat_size = material_str.size();
		if (mat_size <= 0)
		{
			llwarns << "Could not zip LLSD binary content in region: "
					<< region_id << llendl;
			return;
		}

		LLSD::Binary material_bin;
		material_bin.resize(mat_size);
		memcpy(material_bin.data(), material_str.data(), mat_size);

		LLSD post_data = LLSD::emptyMap();
		post_data[MATERIALS_CAP_ZIP_FIELD] = material_bin;

		LLCore::HttpHandler::ptr_t
			handler(new LLMaterialHttpHandler("POST",
											  boost::bind(&LLMaterialMgr::onGetResponse,
														  this, _1, _2,
														  region_id)));
		LL_DEBUGS("Materials") << "POSTing to region '" << regionp->getName()
							   << "' at '"<< cap_url << " for "
							   << mats_data.size() << " materials. Data:\n"
							   << ll_pretty_print_sd(mats_data) << LL_ENDL;
		LLCore::HttpHandle handle;
		handle = LLCoreHttpUtil::requestPostWithLLSD(mHttpRequest, mHttpPolicy,
													 cap_url, post_data,
													 mHttpOptions, mHttpHeaders,
													 handler);

		if (!instanceExists()) return;	// Viewer is being closed down !

		if (handle == LLCORE_HTTP_HANDLE_INVALID)
		{
			LLCore::HttpStatus status = mHttpRequest->getStatus();
			llwarns << "Failed to post materials. Status: " << status.toULong()
					<< " - " << status.toString() << llendl;
		}

		regionp->resetMaterialsCapThrottle();
	}
}

void LLMaterialMgr::processGetAllQueue()
{
	uuid_list_t::iterator loop_rit = mGetAllQueue.begin();
	while (loop_rit != mGetAllQueue.end())
	{
		uuid_list_t::iterator rit = loop_rit++;

		const LLUUID& region_id = *rit;

		LLViewerRegion* regionp = gWorld.getRegionFromID(region_id);
		if (!regionp)
		{
			llwarns << "Unknown region with id " << region_id << llendl;
			clearGetQueues(region_id);		// Invalidates region_id
			continue;
		}
		else if (!regionp->capabilitiesReceived() ||
				 regionp->materialsCapThrottled())
		{
			continue;
		}

		const std::string& url =
			regionp->getCapability(MATERIALS_CAPABILITY_NAME);
		if (url.empty())
		{
			llwarns << "Capability '" << MATERIALS_CAPABILITY_NAME
					<< "' is not defined for region: "
					<< regionp->getIdentity() << llendl;
			clearGetQueues(region_id);		// Invalidates region_id
			continue;
		}

		LL_DEBUGS("Materials") << "GET all for region: " << region_id
							   << " - url: " << url << LL_ENDL;

		gCoros.launch("LLMaterialMgr::processGetAllQueueCoro",
					  boost::bind(&LLMaterialMgr::processGetAllQueueCoro, this,
								  url, region_id));

		regionp->resetMaterialsCapThrottle();
		mGetAllPending.emplace(region_id, LLFrameTimer::getTotalSeconds());
		mGetAllQueue.erase(rit);	// Invalidates region_id
	}
}

void LLMaterialMgr::processGetAllQueueCoro(const std::string& url,
										   LLUUID region_id)
{
	LLSD result = mHttpAdapter->getAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		onGetAllResponse(true, result, region_id);
	}
	else
	{
		onGetAllResponse(false, LLSD(), region_id);
	}
}

void LLMaterialMgr::processPutQueue()
{
	typedef fast_hmap<LLViewerRegion*, LLSD> regionput_req_map_t;
	regionput_req_map_t requests;

	put_queue_t::iterator loop_qit = mPutQueue.begin();
	while (loop_qit != mPutQueue.end())
	{
		put_queue_t::iterator qit = loop_qit++;

		const LLUUID& object_id = qit->first;
		const LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (!objectp)
		{
			llwarns << "Cannot find object " << object_id << llendl;
			mPutQueue.hmap_erase(qit);
			continue;
		}

		LLViewerRegion* regionp = objectp->getRegion();
		if (!regionp)
		{
			llwarns << "Cannot find region for object " << object_id << llendl;
			mPutQueue.hmap_erase(qit);
			continue;
		}
		
		if (!regionp->capabilitiesReceived() ||
			regionp->materialsCapThrottled())
		{
			continue;
		}

		LLSD& faces_data = requests[regionp];

		facematerial_map_t& face_map = qit->second;

		U32 max_entries = regionp->getMaxMaterialsPerTransaction();
		facematerial_map_t::iterator fit = face_map.begin();
		while (face_map.end() != fit && (U32)faces_data.size() < max_entries)
		{
			LLSD face_data = LLSD::emptyMap();
			face_data[MATERIALS_CAP_FACE_FIELD] = LLSD::Integer(fit->first);
			face_data[MATERIALS_CAP_OBJECT_ID_FIELD] =
				LLSD::Integer(objectp->getLocalID());
			if (!fit->second.isNull())
			{
				face_data[MATERIALS_CAP_MATERIAL_FIELD] = fit->second.asLLSD();
			}
			faces_data.append(face_data);
			face_map.erase(fit++);
		}

		if (face_map.empty())
		{
			mPutQueue.hmap_erase(qit);
		}
	}

	for (regionput_req_map_t::const_iterator it = requests.begin(),
											 end = requests.end();
		 it != end; ++it)
	{
		LLViewerRegion* regionp = it->first;
		if (!regionp) continue;	// Paranoia

		const std::string cap_url =
			regionp->getCapability(MATERIALS_CAPABILITY_NAME);
		if (cap_url.empty())
		{
			llwarns << "Capability '" << MATERIALS_CAPABILITY_NAME
					<< "' is not defined for region: "
					<< regionp->getIdentity() << llendl;
			continue;
		}

		LLSD mats_data = LLSD::emptyMap();
		mats_data[MATERIALS_CAP_FULL_PER_FACE_FIELD] = it->second;

		std::string material_str = zip_llsd(mats_data);
		S32 mat_size = material_str.size();
		if (mat_size <= 0)
		{
			llwarns << "Could not zip LLSD binary content" << llendl;
			continue;
		}

		LLSD::Binary material_bin;
		material_bin.resize(mat_size);
		memcpy(material_bin.data(), material_str.data(), mat_size);

		LLSD put_data = LLSD::emptyMap();
		put_data[MATERIALS_CAP_ZIP_FIELD] = material_bin;

		LL_DEBUGS("Materials") << "Put for " << it->second.size()
							   << " faces to region " << regionp->getIdentity()
							   << LL_ENDL;

		LLCore::HttpHandler::ptr_t
			handler(new LLMaterialHttpHandler("PUT",
											  boost::bind(&LLMaterialMgr::onPutResponse,
														  this, _1, _2)));
		LLCore::HttpHandle handle =
			LLCoreHttpUtil::requestPutWithLLSD(mHttpRequest, mHttpPolicy,
											   cap_url, put_data, mHttpOptions,
											   mHttpHeaders, handler);

		if (!instanceExists()) return;	// Viewer is being closed down !

		if (handle == LLCORE_HTTP_HANDLE_INVALID)
		{
			LLCore::HttpStatus status = mHttpRequest->getStatus();
			llwarns << "Failed to put materials. Status: " << status.toULong()
					<< " - " << status.toString() << llendl;
		}

		regionp->resetMaterialsCapThrottle();
	}
}

void LLMaterialMgr::clearGetQueues(const LLUUID& region_id)
{
	mGetQueue.erase(region_id);
	for (get_pending_map_t::iterator it = mGetPending.begin();
		 it != mGetPending.end(); )
	{
		if (region_id == it->first.mRegionId)
		{
			mGetPending.hmap_erase(it++);
		}
		else
		{
			++it;
		}
	}

	mGetAllQueue.erase(region_id);
	mGetAllRequested.erase(region_id);
	mGetAllPending.erase(region_id);
	mGetAllCallbacks.erase(region_id);
}

void LLMaterialMgr::onRegionRemoved(LLViewerRegion* regionp)
{
	clearGetQueues(regionp->getRegionID());
	// Put does not need clearing: objects that cannot be found will clean up
	// in processPutQueue()
}
