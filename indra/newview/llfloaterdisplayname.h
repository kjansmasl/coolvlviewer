/**
 * @file llfloaterdisplayname.h
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LLFLOATERDISPLAYNAME_H
#define LLFLOATERDISPLAYNAME_H

#include "llfloater.h"

class LLFloaterDisplayName final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterDisplayName>
{
	friend class LLUISingleton<LLFloaterDisplayName,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterDisplayName);

public:
	bool postBuild() override;
	void onOpen() override;

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterDisplayName(const LLSD&);

	static void onCacheSetName(bool success, const std::string& reason,
							   const LLSD& content);

	static void onSave(void* data);
	static void onReset(void* data);
	static void onCancel(void* data);
};

#endif
