/**
 * @file llviewquery.cpp
 * @brief Implementation of view query class.
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

#include "linden_common.h"

#include "llviewquery.h"

#include "lluictrl.h"
#include "llview.h"

void LLQuerySorter::operator()(LLView* parent, view_list_t& children) const
{
}

filter_result_t LLLeavesFilter::operator()(const LLView* const view,
										   const view_list_t& children) const
{
	return filter_result_t(children.empty(), true);
}

filter_result_t LLRootsFilter::operator()(const LLView* const view,
										  const view_list_t& children) const
{
	return filter_result_t(true, false);
}

filter_result_t LLVisibleFilter::operator()(const LLView* const view,
										    const view_list_t& children) const
{
	return filter_result_t(view->getVisible(), view->getVisible());
}

filter_result_t LLEnabledFilter::operator()(const LLView* const view,
										    const view_list_t& children) const
{
	return filter_result_t(view->getEnabled(), view->getEnabled());
}

filter_result_t LLTabStopFilter::operator()(const LLView* const view,
										    const view_list_t& children) const
{
	return filter_result_t(view->isCtrl() &&
						  static_cast<const LLUICtrl*>(view)->hasTabStop(),
						  view->canFocusChildren());
}

filter_result_t LLCtrlFilter::operator()(const LLView* const view,
										 const view_list_t& children) const
{
	return filter_result_t(view->isCtrl(), true);
}

//
// LLViewQuery
//

view_list_t LLViewQuery::run(LLView* view) const
{
	view_list_t result;

	// prefilter gets immediate children of view
	filter_result_t pre = runFilters(view, *view->getChildList(), mPreFilters);
	if (!pre.first && !pre.second)
	{
		// Not including ourselves or the children; nothing more to do
		return result;
	}

	view_list_t filtered_children;
	filter_result_t post(true, true);
	if (pre.second)
	{
		// run filters on children
		filterChildren(view, filtered_children);
		// only run post filters if this element passed pre filters so if you
		// failed to pass the pre filter, you can't filter out children in post
		if (pre.first)
		{
			post = runFilters(view, filtered_children, mPostFilters);
		}
	}

	if (pre.first && post.first)
	{
		result.push_back(view);
	}

	if (pre.second && post.second)
	{
		result.insert(result.end(), filtered_children.begin(),
					  filtered_children.end());
	}

	return result;
}

void LLViewQuery::filterChildren(LLView* view,
								 view_list_t& filtered_children) const
{
	LLView::child_list_t views(*(view->getChildList()));
	if (mSorterp)
	{
		(*mSorterp)(view, views); // sort the children per the sorter
	}
	for (LLView::child_list_iter_t iter = views.begin(), end = views.end();
		 iter != end; ++iter)
	{
		view_list_t indiv_children = this->run(*iter);
		filtered_children.insert(filtered_children.end(),
								 indiv_children.begin(), indiv_children.end());
	}
}

filter_result_t LLViewQuery::runFilters(LLView* view,
										const view_list_t& children,
										const filter_list_t& filters) const
{
	filter_result_t result = filter_result_t(true, true);
	for (filter_list_const_iter_t iter = filters.begin(), end = filters.end();
		 iter != end; ++iter)
	{
		filter_result_t filtered = (**iter)(view, children);
		result.first = result.first && filtered.first;
		result.second = result.second && filtered.second;
	}
	return result;
}

class SortByTabOrder : public LLQuerySorter, public LLSingleton<SortByTabOrder>
{
	friend class LLSingleton<SortByTabOrder>;

	void operator()(LLView* parent,
					LLView::child_list_t& children) const override
	{
		children.sort(LLCompareByTabOrder(parent->getCtrlOrder()));
	}
};

LLCtrlQuery::LLCtrlQuery()
:	LLViewQuery()
{
	setSorter(SortByTabOrder::getInstance());
}
