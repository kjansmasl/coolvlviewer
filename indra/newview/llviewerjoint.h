/**
 * @file llviewerjoint.h
 * @brief Implementation of LLViewerJoint class
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

#ifndef LL_LLVIEWERJOINT_H
#define LL_LLVIEWERJOINT_H

#include "llavatarjoint.h"

class LLViewerJoint : public virtual LLAvatarJoint
{
public:
	LLViewerJoint();

	// *TODO: Only used for LLVOAvatarSelf::mScreenp *DOES NOT INITIALIZE*
	// mResetAfterRestoreOldXform*
	LLViewerJoint(const std::string& name, LLJoint* parent = NULL);

	LL_INLINE LLViewerJoint* asViewerJoint() override	{ return this; }

	// Render character hierarchy. Traverses the entire joint hierarchy,
	// setting up transforms and calling the drawShape(). Derived classes may
	// add text/graphic output. Returns the triangle count
	U32 render(F32 pixelArea, bool first_pass = true,
			   bool is_dummy = false) override;

	// Draws the shape attached to a joint. Called by render().
	virtual U32 drawShape(F32 pixelArea, bool first_pass = true,
						  bool is_dummy = false)
	{
		return 0;
	}
};

#endif // LL_LLVIEWERJOINT_H
