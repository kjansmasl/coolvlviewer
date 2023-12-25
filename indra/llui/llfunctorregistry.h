/**
 * @file llfunctorregistry.h
 * @author Kent Quirk
 * @brief Maintains a registry of named callback functors taking a single LLSD parameter
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#ifndef LL_LLFUNCTORREGISTRY_H
#define LL_LLFUNCTORREGISTRY_H

#include "boost/function.hpp"

#include "hbfastmap.h"
#include "llsd.h"
#include "llsingleton.h"

// LLFunctorRegistry maintains a collection of named functors in a singleton.
// We wanted to be able to persist notifications with their callbacks across
// restarts of the viewer; we could not store functors that way. Using this
// registry, systems that require a functor to be maintained long term can
// register it at system startup, and then pass in the functor by name. 

template<typename FUNCTOR_TYPE>
class LLFunctorRegistry : public LLSingleton<LLFunctorRegistry<FUNCTOR_TYPE> >
{
	friend class LLSingleton<LLFunctorRegistry>;

protected:
	LOG_CLASS(LLFunctorRegistry);

public:
	typedef FUNCTOR_TYPE ResponseFunctor;
	typedef typename flat_hmap<std::string, FUNCTOR_TYPE> map_t;

	void registerFunctor(const std::string& name, ResponseFunctor f)
	{
		if (mMap.count(name))
		{
			llerrs << "Attempt to store duplicate name: " << name << llendl;
		}
		mMap.emplace(name, f);
	}

	void unregisterFunctor(const std::string& name)
	{
		if (!mMap.erase(name))
		{
			llwarns << "Could not find: " << name << llendl;
		}
	}

	FUNCTOR_TYPE getFunctor(const std::string& name) const
	{
		auto it = mMap.find(name);
		if (it != mMap.end())
		{
			return it->second;
		}
		// This a common/normal occurrence: do not use llwarns here. HB
		LL_DEBUGS("FunctorRegistry") << "Could not find: " << name << LL_ENDL;
		return do_nothing;
	}

private:
	LLFunctorRegistry() = default;

	static void do_nothing(const LLSD&, const LLSD& payload)
	{
		LL_DEBUGS("FunctorRegistry") << "Payload: " << payload << LL_ENDL;
	}

private:
	map_t mMap;
};

template<typename FUNCTOR_TYPE>
class LLFunctorRegistration
{
public:
	LLFunctorRegistration(const std::string& name, FUNCTOR_TYPE functor) 
	{
		LLFunctorRegistry<FUNCTOR_TYPE>::getInstance()->registerFunctor(name,
																		functor);
	}
};

#endif//LL_LLFUNCTORREGISTRY_H
