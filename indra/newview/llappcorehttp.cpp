/**
 * @file llappcorehttp.cpp
 * @brief
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab. Terms of
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

#if 0	// Local SSL verification not implemented/used: we rely on libcurl
# include "curl/curl.h"
# include "openssl/x509_vfy.h"
# include "openssl/ssl.h"

# include "llsecapi.h"
#endif

#include "curl/curlver.h"

#include "llappcorehttp.h"

#include "lldir.h"

#include "llappviewer.h"
#include "llgridmanager.h"		// For gIsInSecondLife
#include "lltexturefetch.h"
#include "llviewercontrol.h"

// Here is where we begin to get our connection usage under control. This
// establishes llcorehttp policy classes that, among other things, limit the
// maximum number of connections to outside services. Each of the entries below
// maps to a policy class and has a limit, sometimes configurable, of how many
// connections can be open at a time.

constexpr F64 MAX_THREAD_WAIT_TIME = 10.0;

static const struct
{
	U32			mDefault;
	U32			mMin;
	U32			mMax;
	U32			mRate;
	bool		mPipelined;
	std::string	mKey;
	const char*	mUsage;
} init_data[LLAppCoreHttp::AP_COUNT] =
{
	{	// AP_DEFAULT
		8,		4,		8,		0,		false,
		"",
		"other"
	},
	{	// AP_TEXTURE
		12,		2,		32,		0,		true,
		"TextureFetchConcurrency",
		"texture fetch"
	},
	{	// AP_MESH1
		32,		1,		128,	0,		false,
		"MeshMaxConcurrentRequests",
		"mesh fetch"
	},
	{	// AP_MESH2
		16,		1,		32,		0,		true,
		"Mesh2MaxConcurrentRequests",
		"mesh2 fetch"
	},
	{	// AP_LARGE_MESH
		4,		1,		8,		0,		false,
		"",
		"large mesh fetch"
	},
	{	// AP_ASSETS
		8,		2,		32,		0,		true,
		"AssetFetchConcurrency",
		"asset fetch"
	},
	{	// AP_UPLOADS
		2,		1,		8,		0,		false,
		"",
		"asset upload"
	},
	{	// AP_LONG_POLL
		32,		32,		32,		0,		false,
		"",
		"long poll"
	},
	{	// AP_INVENTORY
		8,		1,		16,		0,		true,
		"",
		"inventory"
	},
	{ // AP_MATERIALS
		2,		1,		8,		0,		false,
		"MaterialFetchConcurrency",
		"material manager requests"
	},
	{ // AP_AGENT
		2,		1,		32,		0,		false,
		"Agent",
		"Agent requests"
	}
};
constexpr S32 init_data_size = LL_ARRAY_SIZE(init_data);

void setting_changed()
{
	gAppViewerp->getAppCoreHttp().refreshSettings(false);
}

LLAppCoreHttp::HttpClass::HttpClass()
:	mPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID),
	mConnLimit(0U),
	mPipelined(false)
{
}

LLAppCoreHttp::LLAppCoreHttp()
:	mRequest(NULL),
	mStopHandle(LLCORE_HTTP_HANDLE_INVALID),
	mStopRequested(0.0),
	mStopped(false),
#if LL_CURL_BUG
	mPipelinedTempOff(false),
	mRestartPipelined(0.f),
#endif
	mPipelined(true)
{
}

LLAppCoreHttp::~LLAppCoreHttp()
{
	delete mRequest;
	mRequest = NULL;
}

void LLAppCoreHttp::init()
{
	LLCore::LLHttp::initialize();

#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 54
	LLCore::LLHttp::gEnabledHTTP2 = gSavedSettings.getBool("EnableHTTP2");
#else
	LLControlVariable* controlp = gSavedSettings.getControl("EnableHTTP2");
	if (controlp)
	{
		controlp->setHiddenFromUser(true);
	}
#endif

	LLCore::HttpStatus status = LLCore::HttpRequest::createService();
	if (!status)
	{
		llerrs << "Failed to initialize HTTP services. Reason: "
			   << status.toString() << llendl;
	}

	// Point to our certs or SSH/https: will fail on connect
	status =
		LLCore::HttpRequest::setStaticPolicyOption(LLCore::HttpRequest::PO_CA_FILE,
												   LLCore::HttpRequest::GLOBAL_POLICY_ID,
												   gDirUtilp->getCRTFile(),
												   NULL);
	if (!status)
	{
		llerrs << "Failed to set CA File for HTTP services. Reason: "
			   << status.toString() << llendl;
	}

	// Establish HTTP Proxy if desired.
	status =
		LLCore::HttpRequest::setStaticPolicyOption(LLCore::HttpRequest::PO_LLPROXY,
												   LLCore::HttpRequest::GLOBAL_POLICY_ID,
												   1, NULL);
	if (!status)
	{
		llwarns << "Failed to set HTTP proxy for HTTP services. Reason: "
				<< status.toString() << llendl;
	}

#if 0	// Not yet implemented/used (let libcurl do the job)
	// Set up SSL Verification call back.
	status =
		LLCore::HttpRequest::setStaticPolicyOption(LLCore::HttpRequest::PO_SSL_VERIFY_CALLBACK,
												   LLCore::HttpRequest::GLOBAL_POLICY_ID,
												   sslVerify, NULL);
	if (!status)
	{
		llwarns << "Failed to set SSL verification. Reason: "
				<< status.toString() << llendl;
	}
#endif

	// Tracing levels for library & libcurl (note that 2 & 3 are beyond spammy):
	// 0 - None
	// 1 - Basic start, stop simple transitions
	// 2 - libcurl CURLOPT_VERBOSE mode with brief lines
	// 3 - with partial data content
	long trace_level = gSavedSettings.getU32("HttpTraceLevel");
	status =
		LLCore::HttpRequest::setStaticPolicyOption(LLCore::HttpRequest::PO_TRACE,
												   LLCore::HttpRequest::GLOBAL_POLICY_ID,
												   trace_level, NULL);

	// Setup default policy and constrain if directed to
	mHttpClasses[AP_DEFAULT].mPolicy = LLCore::HttpRequest::DEFAULT_POLICY_ID;

	// Setup additional policies based on table and some special rules
	llassert(init_data_size == (S32)AP_COUNT);
	for (S32 i = 0, count = init_data_size; i < count; ++i)
	{
		const EAppPolicy app_policy = (EAppPolicy)i;
		if (app_policy == AP_DEFAULT)
		{
			// Pre-created
			continue;
		}

		mHttpClasses[app_policy].mPolicy =
			LLCore::HttpRequest::createPolicyClass();
		// We have ran out of available HTTP policies. Adjust
		// HTTP_POLICY_CLASS_LIMIT in llcorehttpinternal.h
		llassert(mHttpClasses[app_policy].mPolicy !=
					LLCore::HttpRequest::INVALID_POLICY_ID);
		if (!mHttpClasses[app_policy].mPolicy)
		{
			// Use default policy (but do not accidentally modify default)
			llwarns << "Failed to create HTTP policy class for "
					<< init_data[i].mUsage << ". Using default policy."
					<< llendl;
			mHttpClasses[app_policy].mPolicy =
				mHttpClasses[AP_DEFAULT].mPolicy;
			continue;
		}
	}

	// Need a request object to handle dynamic options before setting them
	mRequest = new LLCore::HttpRequest;

	// Apply initial settings
	refreshSettings(true);

	// Kick the thread
	status = LLCore::HttpRequest::startThread();
	if (!status)
	{
		llerrs << "Failed to start HTTP servicing thread. Reason: "
			   << status.toString() << llendl;
	}

	// Signal for global pipelining preference from settings
	LLControlVariablePtr ctrl = gSavedSettings.getControl("HttpPipeliningSL");
	if (ctrl.notNull())
	{
		mPipelinedSignal =
			ctrl->getSignal()->connect(boost::bind(&setting_changed));
	}
	else
	{
		llwarns << "Unable to set signal on global setting: HttpPipeliningSL"
				<< llendl;
	}
	ctrl = gSavedSettings.getControl("HttpPipeliningOS");
	if (ctrl.notNull())
	{
		mOSPipelinedSignal =
			ctrl->getSignal()->connect(boost::bind(&setting_changed));
	}
	else
	{
		llwarns << "Unable to set signal on global setting: HttpPipeliningOS"
				<< llendl;
	}

	// Register signals for settings and state changes
	for (S32 i = 0, count = init_data_size; i < count; ++i)
	{
		const EAppPolicy app_policy = (EAppPolicy)i;

		std::string setting_name = init_data[i].mKey;
		if (!setting_name.empty() &&
			gSavedSettings.controlExists(setting_name.c_str()))
		{
			ctrl = gSavedSettings.getControl(setting_name.c_str());
			if (ctrl.notNull())
			{
				mHttpClasses[app_policy].mSettingsSignal =
					ctrl->getSignal()->connect(boost::bind(&setting_changed));
			}
			else
			{
				llwarns << "Unable to set signal on global setting: "
						<< setting_name << llendl;
			}
		}
	}
}

namespace
{
	// The NoOpDeletor is used when wrapping LLAppCoreHttp in a smart pointer
	// below for passage into the LLCore::Http libararies. When the smart
	// pointer is destroyed, no action will be taken since we do not in this
	// case want the entire LLAppCoreHttp object to be destroyed at the end of
	// the call.
	void NoOpDeletor(LLCore::HttpHandler*)	{}
}

void LLAppCoreHttp::requestStop()
{
	llassert_always(mRequest);

	mStopHandle =
		mRequest->requestStopThread(LLCore::HttpHandler::ptr_t(this,
															   NoOpDeletor));
	if (mStopHandle != LLCORE_HTTP_HANDLE_INVALID)
	{
		mStopRequested = LLTimer::getTotalSeconds();
	}
}

void LLAppCoreHttp::cleanup()
{
	if (mStopHandle == LLCORE_HTTP_HANDLE_INVALID)
	{
		// Should have been started already...
		requestStop();
	}

	if (mStopHandle == LLCORE_HTTP_HANDLE_INVALID)
	{
		llwarns << "Attempting to cleanup HTTP services without thread shutdown"
				<< llendl;
	}
	else
	{
		while (!mStopped &&
			   LLTimer::getTotalSeconds() < mStopRequested + MAX_THREAD_WAIT_TIME)
		{
			mRequest->update(200000);
			ms_sleep(50);
		}
		if (!mStopped)
		{
			llwarns << "Attempting to cleanup HTTP services with thread shutdown incomplete"
					<< llendl;
		}
	}

	for (S32 i = 0, count = LL_ARRAY_SIZE(mHttpClasses); i < count; ++i)
	{
		mHttpClasses[i].mSettingsSignal.disconnect();
	}
	mPipelinedSignal.disconnect();
	mOSPipelinedSignal.disconnect();

	delete mRequest;
	mRequest = NULL;

	LLCore::HttpStatus status = LLCore::HttpRequest::destroyService();
	if (!status)
	{
		llwarns << "Failed to shutdown HTTP services, continuing. Reason: "
				<< status.toString() << llendl;
	}
}

bool LLAppCoreHttp::isPipeliningOn()
{
	static LLCachedControl<bool> sl_ok(gSavedSettings, "HttpPipeliningSL");
	static LLCachedControl<bool> os_ok(gSavedSettings, "HttpPipeliningOS");
	bool pipelined;
	if (gIsInSecondLife)
	{
		pipelined = sl_ok;
	}
	else
	{
		pipelined = os_ok;
	}
#if LL_CURL_BUG
	return pipelined && !mPipelinedTempOff;
#else
	return pipelined;
#endif
}

void LLAppCoreHttp::refreshSettings(bool initial)
{
	LLCore::HttpStatus status;

	// Global pipelining setting. Defaults to true (in ctor) if absent.
	bool pipeline_changed = false;
	bool pipelined = isPipeliningOn();
	if (pipelined != mPipelined)
	{
		mPipelined = pipelined;
		pipeline_changed = true;
	}
	if (initial || pipeline_changed)
	{
		llinfos << "HTTP pipelining is" << (initial ? " " : " now ")
				<< (pipelined ? "enabled" : "disabled") << llendl;
	}

	for (S32 i = 0, count = init_data_size; i < count; ++i)
	{
		const EAppPolicy app_policy = (EAppPolicy)i;

		// Init-time only, can use the static setters here
		if (initial && init_data[i].mRate)
		{
			// Set any desired throttle
			status =
				LLCore::HttpRequest::setStaticPolicyOption(LLCore::HttpRequest::PO_THROTTLE_RATE,
														   mHttpClasses[app_policy].mPolicy,
														   init_data[i].mRate,
														   NULL);
			if (!status)
			{
				llwarns << "Unable to set " << init_data[i].mUsage
						<< " throttle rate. Reason: " << status.toString()
						<< llendl;
			}
		}

		// Init or run-time settings. Must use the queued request API.

		// Pipelining changes
		if (initial || pipeline_changed)
		{
			bool to_pipeline = mPipelined && init_data[i].mPipelined;
			if (to_pipeline != mHttpClasses[app_policy].mPipelined)
			{
				// Pipeline election changing, set dynamic option via request
				long new_depth = to_pipeline ? PIPELINING_DEPTH : 0L;
				LLCore::HttpHandle handle =
					mRequest->setPolicyOption(LLCore::HttpRequest::PO_PIPELINING_DEPTH,
											  mHttpClasses[app_policy].mPolicy,
											  new_depth,
											  LLCore::HttpHandler::ptr_t());
				if (handle == LLCORE_HTTP_HANDLE_INVALID)
				{
					status = mRequest->getStatus();
					llwarns << "Unable to set " << init_data[i].mUsage
							<< " pipelining. Reason: " << status.toString()
							<< llendl;
				}
				else
				{
					LL_DEBUGS("CoreHttp") << "Changed " << init_data[i].mUsage
										  << " pipelining. New value: "
										  << new_depth << LL_ENDL;
					mHttpClasses[app_policy].mPipelined = to_pipeline;
				}
			}
		}

		// Get target connection concurrency value
		U32 setting = init_data[i].mDefault;
		std::string setting_name = init_data[i].mKey;
		if (!setting_name.empty() &&
			gSavedSettings.controlExists(setting_name.c_str()))
		{
			U32 new_setting = gSavedSettings.getU32(setting_name.c_str());
			if (new_setting)
			{
				// Treat zero settings as an ask for default
				setting = llclamp(new_setting, init_data[i].mMin,
								  init_data[i].mMax);
			}
		}

		if (initial || pipeline_changed ||
			setting != mHttpClasses[app_policy].mConnLimit)
		{
			// Set it and report. Strategies depend on pipelining:
			//
			// No Pipelining. llcorehttp manages connections itself based on
			// the PO_CONNECTION_LIMIT setting. Set both limits to the same
			// value for logical consistency. In the future, may hand over
			// connection management to libcurl after the connection cache has
			// been better vetted.
			//
			// Pipelining. libcurl is allowed to manage connections to a great
			// degree. Steady state will connection limit based on the per-host
			// setting. Transitions (region crossings, new avatars, etc) can
			// request additional outbound connections to other servers via 2x
			// total connection limit.
			U32 limit = mHttpClasses[app_policy].mPipelined ? 2 * setting
															: setting;
			LLCore::HttpHandle handle =
				mRequest->setPolicyOption(LLCore::HttpRequest::PO_CONNECTION_LIMIT,
										  mHttpClasses[app_policy].mPolicy,
										  (long)limit,
										  LLCore::HttpHandler::ptr_t());
			if (handle == LLCORE_HTTP_HANDLE_INVALID)
			{
				status = mRequest->getStatus();
				llwarns << "Unable to set " << init_data[i].mUsage
						<< " concurrency. Reason: " << status.toString()
						<< llendl;
			}
			else
			{
				handle =
					mRequest->setPolicyOption(LLCore::HttpRequest::PO_PER_HOST_CONNECTION_LIMIT,
											  mHttpClasses[app_policy].mPolicy,
											  setting,
											  LLCore::HttpHandler::ptr_t());
				if (handle == LLCORE_HTTP_HANDLE_INVALID)
				{
					status = mRequest->getStatus();
					llwarns << "Unable to set " << init_data[i].mUsage
							<< " per-host concurrency. Reason: "
							<< status.toString() << llendl;
				}
				else
				{
					LL_DEBUGS("CoreHttp") << "Changed " << init_data[i].mUsage
										  << " concurrency. New value: "
										  << setting << LL_ENDL;
					mHttpClasses[app_policy].mConnLimit = setting;
					if (initial && setting != init_data[i].mDefault)
					{
						llinfos << "Application settings overriding default "
								<< init_data[i].mUsage
								<< " concurrency. New value: " << setting
								<< llendl;
					}
				}
			}
		}
	}
}

#if LL_CURL_BUG
void LLAppCoreHttp::setPipelinedTempOff(bool turn_off)
{
	if (turn_off)
	{
		mRestartPipelined = gFrameTimeSeconds + 30.f;
		llwarns << "Temporarily disabling HTTP pipelining" << llendl;
	}
	else
	{
		llinfos << "HTTP pipelining re-enabled" << llendl;
		mRestartPipelined = 0.f;
	}
	mPipelinedTempOff = turn_off;
	refreshSettings();
}

void LLAppCoreHttp::checkPipelinedTempOff()
{
	if (mPipelinedTempOff && mRestartPipelined < gFrameTimeSeconds)
	{
		setPipelinedTempOff(false);
	}
}
#endif

LLCore::HttpStatus LLAppCoreHttp::sslVerify(const std::string& url,
											const LLCore::HttpHandler::ptr_t& handler,
											void* userdata)
{
#if 0	// Not yet implemented
	if (gDisconnected)
	{
		return LLCore::HttpStatus(LLCore::HttpStatus::EXT_CURL_EASY,
								  CURLE_OPERATION_TIMEDOUT);
	}

	X509_STORE_CTX* ctx = (X509_STORE_CTX*)userdata;
	LLCore::HttpStatus result;
	LLPointer<LLCertificateStore> store =
		gSecAPIHandler->getCertificateStore("");
	LLPointer<LLCertificateChain> chain =
		gSecAPIHandler->getCertificateChain(ctx);
	LLSD validation_params = LLSD::emptyMap();
	LLURI uri(url);

	validation_params[CERT_HOSTNAME] = uri.hostName();

	// *TODO: In the case of an exception while validating the cert, we need a
	// way to pass the offending(?) cert back out. *Rider*

	try
	{
		// Do not validate hostname. Let libcurl do it instead. That way, it
		// will handle redirects
		store->validate(VALIDATION_POLICY_SSL & ~VALIDATION_POLICY_HOSTNAME,
						chain, validation_params);
	}
	catch (LLCertValidationTrustException& cert_exception)
	{
		// This exception is is handled differently than the general cert
		// exceptions, as we allow the user to actually add the certificate
		// for trust. Therefore we pass back a different error code
		// NOTE: We are currently 'wired' to pass around CURL error codes. This
		// is somewhat clumsy, as we may run into errors that do not map
		// directly to curl error codes. Should be refactored with login
		// refactoring, perhaps.
		result = LLCore::HttpStatus(LLCore::HttpStatus::EXT_CURL_EASY,
									CURLE_SSL_CACERT);
		result.setMessage(cert_exception.getMessage());
		LLPointer<LLCertificate> cert = cert_exception.getCert();
		cert->ref(); // adding an extra ref here
		result.setErrorData(cert.get());
		// We should probably have a more generic way of passing information
		// back to the error handlers.
	}
	catch (LLCertException& cert_exception)
	{
		result = LLCore::HttpStatus(LLCore::HttpStatus::EXT_CURL_EASY,
									CURLE_SSL_PEER_CERTIFICATE);
		result.setMessage(cert_exception.getMessage());
		LLPointer<LLCertificate> cert = cert_exception.getCert();
		cert->ref();	// adding an extra ref here
		result.setErrorData(cert.get());
	}
	catch (...)
	{
		// Any other odd error, we just handle as a connect error.
		result = LLCore::HttpStatus(LLCore::HttpStatus::EXT_CURL_EASY,
									CURLE_SSL_CONNECT_ERROR);
	}
#else
	LLCore::HttpStatus result(LLCore::HttpStatus::EXT_CURL_EASY, CURLE_OK);
#endif

	return result;
}
