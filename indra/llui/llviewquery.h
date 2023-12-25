/**
 * @file llviewquery.h
 * @brief Query algorithm for flattening and filtering the view hierarchy.
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

#ifndef LL_LLVIEWQUERY_H
#define LL_LLVIEWQUERY_H

#include <list>

#include "llsingleton.h"

class LLView;

typedef std::list<LLView*> view_list_t;
typedef std::pair<bool, bool> filter_result_t;

// Abstract base class for all query filters.
class LLQueryFilter
{
public:
	virtual ~LLQueryFilter() = default;
	virtual filter_result_t operator()(const LLView* const view,
									   const view_list_t& children) const = 0;
};

class LLQuerySorter
{
public:
	virtual ~LLQuerySorter() = default;
	virtual void operator()(LLView* parent, view_list_t &children) const;
};

class LLLeavesFilter : public LLQueryFilter, public LLSingleton<LLLeavesFilter>
{
	friend class LLSingleton<LLLeavesFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

class LLRootsFilter : public LLQueryFilter, public LLSingleton<LLRootsFilter>
{
	friend class LLSingleton<LLRootsFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

class LLVisibleFilter : public LLQueryFilter, public LLSingleton<LLVisibleFilter>
{
	friend class LLSingleton<LLVisibleFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

class LLEnabledFilter : public LLQueryFilter, public LLSingleton<LLEnabledFilter>
{
	friend class LLSingleton<LLEnabledFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

class LLTabStopFilter : public LLQueryFilter, public LLSingleton<LLTabStopFilter>
{
	friend class LLSingleton<LLTabStopFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

class LLCtrlFilter : public LLQueryFilter, public LLSingleton<LLCtrlFilter>
{
	friend class LLSingleton<LLCtrlFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override;
};

template <class T>
class LLWidgetTypeFilter : public LLQueryFilter
{
	LL_INLINE filter_result_t operator()(const LLView* const view,
										 const view_list_t& children) const override
	{
		return filter_result_t(dynamic_cast<const T*>(view) != NULL, true);
	}
};

// Algorithm for flattening
class LLViewQuery
{
public:
	typedef std::list<const LLQueryFilter*>	filter_list_t;
	typedef filter_list_t::iterator			filter_list_iter_t;
	typedef filter_list_t::const_iterator	filter_list_const_iter_t;

	LL_INLINE LLViewQuery()
	:	mSorterp(NULL)
	{
	}

	virtual ~LLViewQuery() = default;

	LL_INLINE void addPreFilter(const LLQueryFilter* prefilter)
	{
		mPreFilters.push_back(prefilter);
	}

	LL_INLINE void addPostFilter(const LLQueryFilter* postfilter)
	{
		mPostFilters.push_back(postfilter);
	}

	LL_INLINE const filter_list_t& getPreFilters() const	{ return mPreFilters; }
	LL_INLINE const filter_list_t& getPostFilters() const	{ return mPostFilters; }

	LL_INLINE void setSorter(const LLQuerySorter* sorter)	{ mSorterp = sorter; }
	LL_INLINE const LLQuerySorter* getSorter() const		{ return mSorterp; }

	view_list_t run(LLView* view) const;

	// Syntactic sugar
	LL_INLINE view_list_t operator()(LLView* view) const	{ return run(view); }

	// Override this method to provide iteration over other types of children
	virtual void filterChildren(LLView* view,
								view_list_t& filtered_children) const;

private:
	filter_result_t runFilters(LLView* view, const view_list_t& children,
							   const filter_list_t& filters) const;

private:
	filter_list_t			mPreFilters;
	filter_list_t			mPostFilters;
	const LLQuerySorter*	mSorterp;
};

class LLCtrlQuery : public LLViewQuery
{
public:
	LLCtrlQuery();
};

#endif // LL_LLVIEWQUERY_H
