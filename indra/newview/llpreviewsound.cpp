/**
 * @file llpreviewsound.cpp
 * @brief LLPreviewSound class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llpreviewsound.h"

#include "llaudioengine.h"
#include "llbutton.h"
#include "lllineeditor.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"	// send_sound_trigger()

constexpr F32 SOUND_GAIN = 1.f;

//static
S32 LLPreviewSound::sPreviewSoundCount = 0;

LLPreviewSound::LLPreviewSound(const std::string& name, const LLRect& rect,
							   const std::string& title,
							   const LLUUID& item_uuid,
							   const LLUUID& object_uuid)
:	LLPreview(name, rect, title, item_uuid, object_uuid)
{
	++sPreviewSoundCount;

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preview_sound.xml");

	childSetAction("Sound play btn", playSound, this);
	childSetAction("Sound audition btn", auditionSound, this);

	LLButton* button = getChild<LLButton>("Sound play btn");
	button->setSoundFlags(LLView::SILENT);

	button = getChild<LLButton>("Sound audition btn");
	button->setSoundFlags(LLView::SILENT);

	childSetCommitCallback("desc", LLPreview::onText, this);
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);

	const LLInventoryItem* item = getItem();
	if (item)	// May be null (e.g. during prim contents fetches)...
	{
		childSetText("desc", item->getDescription());
		if (gAudiop)
		{
			// preload the sound
			gAudiop->preloadSound(item->getAssetUUID());
		}
	}
	else
	{
		childSetText("desc", std::string("(loading...)"));
	}

	setTitle(title);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}
}

LLPreviewSound::~LLPreviewSound()
{
	--sPreviewSoundCount;
}

// static
void LLPreviewSound::playSound(void* userdata)
{
	LLPreviewSound* self = (LLPreviewSound*)userdata;
	if (self)
	{
		const LLInventoryItem* item = self->getItem();
		if (item && gAudiop)
		{
			send_sound_trigger(item->getAssetUUID(), SOUND_GAIN);
		}
	}
}

// static
void LLPreviewSound::auditionSound(void* userdata)
{
	LLPreviewSound* self = (LLPreviewSound*)userdata;
	if (self)
	{
		const LLInventoryItem* item = self->getItem();
		if (item && gAudiop)
		{
			gAudiop->triggerSound(item->getAssetUUID(), gAgentID, SOUND_GAIN,
								  LLAudioEngine::AUDIO_TYPE_SFX);
		}
	}
}
