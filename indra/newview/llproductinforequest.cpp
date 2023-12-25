/**
 * @file llproductinforequest.cpp
 * @author Kent Quirk
 * @brief Get region type descriptions (translation from SKU to description)
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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

#include "llproductinforequest.h"

#include "llcorehttputil.h"
#include "lltrans.h"

#include "llagent.h"  // for gAgent

LLProductInfoRequestManager::LLProductInfoRequestManager()
:	mSkuDescriptions()
{
}

void LLProductInfoRequestManager::initSingleton()
{
	const std::string& url = gAgent.getRegionCapability("ProductInfoRequest");
	if (url.empty())
	{
		return;
	}
	gCoros.launch("LLProductInfoRequestManager::getLandDescriptionsCoro",
				  boost::bind(&LLProductInfoRequestManager::getLandDescriptionsCoro,
							  this, url));
}

std::string LLProductInfoRequestManager::getDescriptionForSku(const std::string& sku)
{
	// The description LLSD is an array of maps; each array entry has a map
	// with 3 fields: description, name, and sku
	for (LLSD::array_const_iterator it = mSkuDescriptions.beginArray(),
									end = mSkuDescriptions.endArray();
		 it != end; ++it)
	{
		LL_DEBUGS("ProductInfoRequestManager") <<  (*it)["sku"].asString()
											   << " = "
											   << (*it)["description"].asString()
											   << LL_ENDL;
		if ((*it)["sku"].asString() == sku)
		{
			return (*it)["description"].asString();
		}
	}
	return LLTrans::getString("unknown");
}

void LLProductInfoRequestManager::getLandDescriptionsCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("ProductInfoRequest");
	LLSD result = adapter.getAndSuspend(url);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Failure to fetch land SKU: " << status.toString()
				<< llendl;
	}
	else if (result.has(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT) &&
			 result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT].isArray())
	{
		mSkuDescriptions = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT];
	}
	else
	{
		llwarns << "Land SKU description response is malformed" << llendl;
	}
}
