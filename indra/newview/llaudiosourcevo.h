/**
 * @file llaudiosourcevo.h
 * @author Douglas Soo, James Cook
 * @brief Audio sources attached to viewer objects
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

#ifndef LL_LLAUDIOSOURCEVO_H
#define LL_LLAUDIOSOURCEVO_H

#include "llaudioengine.h"
#include "llpointer.h"

#include "llviewerobject.h"

class LLViewerObject;

class LLAudioSourceVO final : public LLAudioSource
{
public:
	LLAudioSourceVO(const LLUUID& sound_id, const LLUUID& owner_id,
					F32 gain, LLViewerObject* objectp);

	~LLAudioSourceVO() override;

	void update() override;
	void setGain(F32 gain) override;

	void checkCutOffRadius();

	LL_INLINE LLPointer<LLViewerObject> getObject()	{ return mObjectp; }

private:
	bool isInCutOffRadius(const LLVector3d& pos_global, F32 cutoff) const;
	void updateMute();

private:
	LLPointer<LLViewerObject>	mObjectp;
	F32							mLastUpdate;
};

#endif // LL_LLAUDIOSOURCEVO_H
