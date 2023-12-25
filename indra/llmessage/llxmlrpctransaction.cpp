/**
 * @file llxmlrpctransaction.cpp
 * @brief LLXMLRPCTransaction and related class implementations
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include <memory>

#include "xmlrpc-epi/xmlrpc.h"
// <xmlrpc-epi/queue.h> contains a harmful #define queue xmlrpc_queue. This
// breaks any use of std::queue. Ditch that #define: if any of our code wants
// to reference xmlrpc_queue, let it reference it directly.
#if defined(queue)
# undef queue
#endif

#include "llxmlrpctransaction.h"

#include "llcorebufferarray.h"
#include "llcorehttphandler.h"
#include "llcorehttprequest.h"
#include "llcorehttpresponse.h"
#include "llhttpconstants.h"

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCValue class
///////////////////////////////////////////////////////////////////////////////

LLXMLRPCValue LLXMLRPCValue::operator[](const char* id) const
{
	return LLXMLRPCValue(XMLRPC_VectorGetValueWithID(mV, id));
}

std::string LLXMLRPCValue::asString() const
{
	const char* s = XMLRPC_GetValueString(mV);
	return s ? s : "";
}

int LLXMLRPCValue::asInt() const
{
	return XMLRPC_GetValueInt(mV);
}

bool LLXMLRPCValue::asBool() const
{
	return XMLRPC_GetValueBoolean(mV) != 0;
}

double LLXMLRPCValue::asDouble() const
{
	return XMLRPC_GetValueDouble(mV);
}

LLXMLRPCValue LLXMLRPCValue::rewind()
{
	return LLXMLRPCValue(XMLRPC_VectorRewind(mV));
}

LLXMLRPCValue LLXMLRPCValue::next()
{
	return LLXMLRPCValue(XMLRPC_VectorNext(mV));
}

bool LLXMLRPCValue::isValid() const
{
	return mV != NULL;
}

LLXMLRPCValue LLXMLRPCValue::createArray()
{
	return LLXMLRPCValue(XMLRPC_CreateVector(NULL, xmlrpc_vector_array));
}

LLXMLRPCValue LLXMLRPCValue::createStruct()
{
	return LLXMLRPCValue(XMLRPC_CreateVector(NULL, xmlrpc_vector_struct));
}

void LLXMLRPCValue::append(LLXMLRPCValue& v)
{
	XMLRPC_AddValueToVector(mV, v.mV);
}

void LLXMLRPCValue::appendString(const std::string& v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueString(NULL, v.c_str(), 0));
}

void LLXMLRPCValue::appendInt(int v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueInt(NULL, v));
}

void LLXMLRPCValue::appendBool(bool v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueBoolean(NULL, v));
}

void LLXMLRPCValue::appendDouble(double v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueDouble(NULL, v));
}

void LLXMLRPCValue::append(const char* id, LLXMLRPCValue& v)
{
	XMLRPC_SetValueID(v.mV, id, 0);
	XMLRPC_AddValueToVector(mV, v.mV);
}

void LLXMLRPCValue::appendString(const char* id, const std::string& v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueString(id, v.c_str(), 0));
}

void LLXMLRPCValue::appendInt(const char* id, int v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueInt(id, v));
}

void LLXMLRPCValue::appendBool(const char* id, bool v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueBoolean(id, v));
}

void LLXMLRPCValue::appendDouble(const char* id, double v)
{
	XMLRPC_AddValueToVector(mV, XMLRPC_CreateValueDouble(id, v));
}

void LLXMLRPCValue::cleanup()
{
	XMLRPC_CleanupValue(mV);
	mV = NULL;
}

XMLRPC_VALUE LLXMLRPCValue::getValue() const
{
	return mV;
}

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCTransaction::Handler sub-class
///////////////////////////////////////////////////////////////////////////////

class LLXMLRPCTransaction::Handler : public LLCore::HttpHandler
{
protected:
	LOG_CLASS(LLXMLRPCTransaction::Handler);

public:
	typedef std::shared_ptr<LLXMLRPCTransaction::Handler> ptr_t;

	Handler(LLCore::HttpRequest::ptr_t& request,
			LLXMLRPCTransaction::Impl* impl)
	:	mImpl(impl),
		mRequest(request)
	{
	}

	virtual ~Handler()	{}

	virtual void onCompleted(LLCore::HttpHandle handle,
							 LLCore::HttpResponse* response);

private:
	LLXMLRPCTransaction::Impl*	mImpl;
	LLCore::HttpRequest::ptr_t	mRequest;
};

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCTransaction::Impl sub-class
///////////////////////////////////////////////////////////////////////////////

class LLXMLRPCTransaction::Impl
{
protected:
	LOG_CLASS(LLXMLRPCTransaction::Impl);

public:
	typedef LLXMLRPCTransaction::EStatus EStatus;

	Impl(const std::string& uri, XMLRPC_REQUEST request, bool use_gzip);
	Impl(const std::string& uri, const std::string& method,
		 LLXMLRPCValue params, bool use_gzip);
	~Impl();

	bool process();

	void setStatus(EStatus code, const std::string& message = "",
				   const std::string& uri = "");
	void setHttpStatus(const LLCore::HttpStatus& status);

private:
	void init(XMLRPC_REQUEST request, bool use_gzip);

public:
	LLCore::HttpRequest::ptr_t					mHttpRequest;
	LLCore::HttpResponse::TransferStats::ptr_t	mTransferStats;
	LLCore::HttpHandle							mPostH;
	Handler::ptr_t								mHandler;
	EStatus										mStatus;
	CURLcode									mCurlCode;
	std::string									mStatusMessage;
	std::string									mStatusURI;
	std::string									mURI;
	std::string									mResponseText;
	XMLRPC_REQUEST								mResponse;

	static std::string							sSupportURL;
	static std::string							sWebsiteURL;
	static std::string							sServerIsDownMsg;
	static std::string							sNotResolvingMsg;
	static std::string							sNotVerifiedMsg;
	static std::string							sConnectErrorMsg;

	static bool									sVerifyCert;
};

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCTransaction::Handler sub-class methods
///////////////////////////////////////////////////////////////////////////////

void LLXMLRPCTransaction::Handler::onCompleted(LLCore::HttpHandle handle,
											   LLCore::HttpResponse* response)
{
	if (!response || !mImpl) return;	// Paranioa

	LLCore::HttpStatus status = response->getStatus();
	if (!status)
	{
		if (status.toULong() != CURLE_SSL_PEER_CERTIFICATE &&
			status.toULong() != CURLE_SSL_CACERT)
		{
			// If we have a curl error that has not already been handled (a non
			// cert error), then generate the error message as appropriate
			mImpl->setHttpStatus(status);
			llwarns << "Error " << status.toHex() << ": " << status.toString()
					<< " - Request URI: " << mImpl->mURI << llendl;
		}
		return;
	}

	mImpl->setStatus(LLXMLRPCTransaction::StatusComplete);
	mImpl->mTransferStats = response->getTransferStats();

	// The contents of a buffer array are potentially noncontiguous, so we
	// will need to copy them into an contiguous block of memory for XMLRPC.
	LLCore::BufferArray* body = response->getBody();
	size_t body_size;
	char* bodydata;
	if (body)
	{
		body_size = body->size();
		bodydata = new char[body_size];
		body->read(0, bodydata, body->size());
	}
	else	// This *does* happen !
	{
		body_size = 0;
		bodydata = new char[1];
		bodydata[0] = 0;
	}
	mImpl->mResponse = XMLRPC_REQUEST_FromXML(bodydata, body_size, NULL);
	LL_DEBUGS("XmlRpc") << "Body: " << std::string(bodydata) << LL_ENDL;
	delete[] bodydata;

	bool has_error = false;
	bool has_fault = false;
	int fault_code = 0;
	std::string	fault_string;

	LLXMLRPCValue error(XMLRPC_RequestGetError(mImpl->mResponse));
	if (error.isValid())
	{
		has_error = true;
		fault_code = error["faultCode"].asInt();
		fault_string = error["faultString"].asString();
	}
	else if (XMLRPC_ResponseIsFault(mImpl->mResponse))
	{
		has_fault = true;
		fault_code = XMLRPC_GetResponseFaultCode(mImpl->mResponse);
		fault_string = XMLRPC_GetResponseFaultString(mImpl->mResponse);
	}

	if (has_error || has_fault)
	{
		mImpl->setStatus(LLXMLRPCTransaction::StatusXMLRPCError);

		llwarns << "XMLRPC " << (has_error ? "error " : "fault ")
				<< fault_code << ": " << fault_string
				<< " - Request URI: " << mImpl->mURI << llendl;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCTransaction::Impl sub-class methods
///////////////////////////////////////////////////////////////////////////////

// Static members
std::string LLXMLRPCTransaction::Impl::sSupportURL;
std::string LLXMLRPCTransaction::Impl::sWebsiteURL;
std::string	LLXMLRPCTransaction::Impl::sServerIsDownMsg;
std::string	LLXMLRPCTransaction::Impl::sNotResolvingMsg;
std::string	LLXMLRPCTransaction::Impl::sNotVerifiedMsg;
std::string	LLXMLRPCTransaction::Impl::sConnectErrorMsg;
bool LLXMLRPCTransaction::Impl::sVerifyCert = true;

LLXMLRPCTransaction::Impl::Impl(const std::string& uri,
								XMLRPC_REQUEST request,
								bool use_gzip)
:	mStatus(LLXMLRPCTransaction::StatusNotStarted),
	mURI(uri),
	mResponse(0)
{
	init(request, use_gzip);
}

LLXMLRPCTransaction::Impl::Impl(const std::string& uri,
								const std::string& method,
								LLXMLRPCValue params, bool use_gzip)
:	mStatus(LLXMLRPCTransaction::StatusNotStarted),
	mURI(uri),
	mResponse(0)
{
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);
	XMLRPC_RequestSetData(request, params.getValue());

	init(request, use_gzip);
    // DEV-28398: without this XMLRPC_RequestFree() call, it looks as though
    // the 'request' object is simply leaked. It's less clear to me whether we
    // should also ask to free request value data (second param 1), since the
    // data come from 'params'.
    XMLRPC_RequestFree(request, 1);
}

// *NOTE: the new LLCore-based code behaves like if the use_gzip bool is always
// true
void LLXMLRPCTransaction::Impl::init(XMLRPC_REQUEST request, bool use_gzip)
{
	if (!mHttpRequest)
	{
		mHttpRequest = DEFAULT_HTTP_REQUEST;
	}

	// LLRefCounted starts with a 1 ref, so do not add a ref in the smart
	// pointer
	LLCore::HttpOptions::ptr_t options = DEFAULT_HTTP_OPTIONS;
	// Be a little impatient about establishing connections.
	options->setTimeout(40L);
	options->setSSLVerifyPeer(sVerifyCert);
	options->setSSLVerifyHost(sVerifyCert);
#if 1
	options->setDNSCacheTimeout(40);
	options->setRetries(3);
#endif

	// LLRefCounted starts with a 1 ref, so do not add a ref in the smart
	// pointer
	LLCore::HttpHeaders::ptr_t headers = DEFAULT_HTTP_HEADERS;
	headers->append(HTTP_OUT_HEADER_CONTENT_TYPE, HTTP_CONTENT_TEXT_XML);

	LLCore::BufferArray::ptr_t body =
		LLCore::BufferArray::ptr_t(new LLCore::BufferArray());

	// *TODO: See if there is a way to serialize to a preallocated buffer
	int request_size = 0;
	char* request_text = XMLRPC_REQUEST_ToXML(request, &request_size);
	body->append(request_text, request_size);
	XMLRPC_Free(request_text);

	mHandler = LLXMLRPCTransaction::Handler::ptr_t(new Handler(mHttpRequest,
															   this));
	mPostH = mHttpRequest->requestPost(LLCore::HttpRequest::DEFAULT_POLICY_ID,
									   mURI, body.get(), options, headers,
									   mHandler);
}

LLXMLRPCTransaction::Impl::~Impl()
{
	if (mResponse)
	{
		XMLRPC_RequestFree(mResponse, 1);
	}
}

bool LLXMLRPCTransaction::Impl::process()
{
	if (!mPostH || !mHttpRequest)
	{
		llwarns << "Transaction failed." << llendl;
		return true;
	}

	switch (mStatus)
	{
		case LLXMLRPCTransaction::StatusComplete:
		case LLXMLRPCTransaction::StatusCURLError:
		case LLXMLRPCTransaction::StatusXMLRPCError:
		case LLXMLRPCTransaction::StatusOtherError:
		{
			return true;
		}

		case LLXMLRPCTransaction::StatusNotStarted:
		{
			setStatus(LLXMLRPCTransaction::StatusStarted);
			break;
		}

		default:
		{
			// continue onward
		}
	}

	LLCore::HttpStatus status = mHttpRequest->update(0);
	if (!status)
	{
		llwarns << "Error (1): " << status.toString() << llendl;
		return false;
	}

	status = mHttpRequest->getStatus();
	if (!status)
	{
		llwarns << "Error (2): " << status.toString() << llendl;
	}

	return false;
}

void LLXMLRPCTransaction::Impl::setStatus(EStatus status,
										  const std::string& message,
										  const std::string& uri)
{
	mStatus = status;
	mStatusMessage = message;
	mStatusURI = uri;

	if (mStatusMessage.empty())
	{
		switch (mStatus)
		{
			case StatusNotStarted:
				mStatusMessage = "(not started)";
				break;

			case StatusStarted:
				mStatusMessage = "(waiting for server response)";
				break;

			case StatusDownloading:
				mStatusMessage = "(reading server response)";
				break;

			case StatusComplete:
				mStatusMessage = "(done)";
				break;

			default:
			{
				// Usually this means that there is a problem with the login
				// server, not with the client. Direct user to status page.
				mStatusMessage = sServerIsDownMsg;
				mStatusURI = sWebsiteURL;
			}
		}
	}
}

void LLXMLRPCTransaction::Impl::setHttpStatus(const LLCore::HttpStatus& status)
{
	CURLcode code = (CURLcode)status.toULong();
	std::string message;
	switch (code)
	{
		case CURLE_COULDNT_RESOLVE_HOST:
			message = sNotResolvingMsg;
			break;

		case CURLE_SSL_CACERT:
		// Note: CURLE_SSL_CACERT and CURLE_SSL_CACERT may expand to the same
		// value in recent curl versions (seen with curl v7.68).
#if CURLE_SSL_CACERT != CURLE_SSL_PEER_CERTIFICATE
		case CURLE_SSL_PEER_CERTIFICATE:
#endif
			message = sNotVerifiedMsg;
			break;

		case CURLE_SSL_CONNECT_ERROR:
			message = sConnectErrorMsg;
			break;

		default:
			break;
	}
	mCurlCode = code;
	setStatus(StatusCURLError, message, sSupportURL);
}

///////////////////////////////////////////////////////////////////////////////
// LLXMLRPCTransaction class proper
///////////////////////////////////////////////////////////////////////////////

LLXMLRPCTransaction::LLXMLRPCTransaction(const std::string& uri,
										 XMLRPC_REQUEST request,
										 bool use_gzip)
:	impl(*new Impl(uri, request, use_gzip))
{
}

LLXMLRPCTransaction::LLXMLRPCTransaction(const std::string& uri,
										 const std::string& method,
										 LLXMLRPCValue params,
										 bool use_gzip)
:	impl(*new Impl(uri, method, params, use_gzip))
{
}

LLXMLRPCTransaction::~LLXMLRPCTransaction()
{
	delete &impl;
}

bool LLXMLRPCTransaction::process()
{
	return impl.process();
}

LLXMLRPCTransaction::EStatus LLXMLRPCTransaction::status(S32* curl_code)
{
	if (curl_code)
	{
		*curl_code = impl.mStatus == StatusCURLError ? impl.mCurlCode
													 : CURLE_OK;
	}
	return impl.mStatus;
}

std::string LLXMLRPCTransaction::statusMessage()
{
	return impl.mStatusMessage;
}

std::string LLXMLRPCTransaction::statusURI()
{
	return impl.mStatusURI;
}

XMLRPC_REQUEST LLXMLRPCTransaction::response()
{
	return impl.mResponse;
}

LLXMLRPCValue LLXMLRPCTransaction::responseValue()
{
	return LLXMLRPCValue(XMLRPC_RequestGetData(impl.mResponse));
}

F64 LLXMLRPCTransaction::transferRate()
{
	if (impl.mStatus != StatusComplete)
	{
		return 0.0;
	}

	double rate_bits_per_sec = impl.mTransferStats->mSpeedDownload * 8.0;

	llinfos << "Buffer size: " << impl.mResponseText.size() << " B"
			<< llendl;
	LL_DEBUGS("AppInit") << "Transfer size: "
						 << impl.mTransferStats->mSizeDownload << " B"
						 << LL_ENDL;
	LL_DEBUGS("AppInit") << "Transfer time: "
						 << impl.mTransferStats->mTotalTime << " s"
						 << LL_ENDL;
	llinfos << "Transfer rate: " << rate_bits_per_sec / 1000.0 << " Kbps"
			<< llendl;

	return rate_bits_per_sec;
}

//static
void LLXMLRPCTransaction::setSupportURL(const std::string& url)
{
	LLXMLRPCTransaction::Impl::sSupportURL = url;
}

//static
void LLXMLRPCTransaction::setWebsiteURL(const std::string& url)
{
	LLXMLRPCTransaction::Impl::sWebsiteURL = url;
}

//static
void LLXMLRPCTransaction::setVerifyCert(bool verify)
{
	LLXMLRPCTransaction::Impl::sVerifyCert = verify;
}

//static
void LLXMLRPCTransaction::setMessages(const std::string& server_down,
									  const std::string& not_resolving,
									  const std::string& not_verified,
									  const std::string& connect_error)
{
	LLXMLRPCTransaction::Impl::sServerIsDownMsg = server_down;
	LLXMLRPCTransaction::Impl::sNotResolvingMsg = not_resolving;
	LLXMLRPCTransaction::Impl::sNotVerifiedMsg = not_verified;
	LLXMLRPCTransaction::Impl::sConnectErrorMsg = connect_error;
}
