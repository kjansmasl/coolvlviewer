/**
 * @file llprimtexturelist.h
 * @brief LLPrimTextureList (virtual) base class
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLPRIMTEXTURELIST_H
#define LL_LLPRIMTEXTURELIST_H

#include <vector>

#include "llmaterial.h"
#include "lluuid.h"
#include "llcolor3.h"
#include "llcolor4.h"

class LLMaterialID;
class LLTextureEntry;

// This is a list of LLTextureEntry*'s because in practice the list elements
// are of some derived class: LLFooTextureEntry
typedef std::vector<LLTextureEntry*> texture_list_t;

class LLPrimTextureList
{
protected:
	LOG_CLASS(LLPrimTextureList);

public:
	// The LLPrimTextureList needs to know what type of LLTextureEntry to
	// generate when it needs a new one, so we may need to set a callback for
	// generating it (or else use the base class default:
	//  static LLPrimTextureEntry::newTextureEntry() )
	// typedef LLTextureEntry* (__stdcall* NewTextureEntryFunction)();
	// static NewTextureEntryFunction sNewTextureEntryCallback;
	static LLTextureEntry* newTextureEntry();
	static void setNewTextureEntryCallback(LLTextureEntry* (*callback)());
	static LLTextureEntry* (*sNewTextureEntryCallback)();

	LLPrimTextureList();
	virtual ~LLPrimTextureList();

	void clear();

	// Clears current entries and copies contents of other_list;
	// this is somewhat expensive, so it must be called explicitly
	void copy(const LLPrimTextureList& other_list);

	// Clears current copies, takes contents of other_list and clears
	// other_list
	void take(LLPrimTextureList& other_list);

	// Copies LLTextureEntry 'te'. Returns TEM_CHANGE_TEXTURE if successful,
	// otherwise TEM_CHANGE_NONE
	S32 copyTexture(U8 index, const LLTextureEntry* te);

	// Takes ownership of LLTextureEntry* 'te', returns TEM_CHANGE_TEXTURE if
	// successful, otherwise TEM_CHANGE_NONE.
	// IMPORTANT: if you use this method you must check the return value
	S32 takeTexture(U8 index, LLTextureEntry* te);

	// Returns pointer to texture at 'index' slot
	LLTextureEntry* getTexture(U8 index) const;

	S32 setID(U8 index, const LLUUID& id);
	S32 setColor(U8 index, const LLColor3& color);
	S32 setColor(U8 index, const LLColor4& color);
	S32 setAlpha(U8 index, F32 alpha);
	S32 setScale(U8 index, F32 s, F32 t);
	S32 setScaleS(U8 index, F32 s);
	S32 setScaleT(U8 index, F32 t);
	S32 setOffset(U8 index, F32 s, F32 t);
	S32 setOffsetS(U8 index, F32 s);
	S32 setOffsetT(U8 index, F32 t);
	S32 setRotation(U8 index, F32 r);
	S32 setBumpShinyFullbright(U8 index, U8 bump);
	S32 setMediaTexGen(U8 index, U8 media);
	S32 setBumpMap(U8 index, U8 bump);
	S32 setBumpShiny(U8 index, U8 bump_shiny);
	S32 setTexGen(U8 index, U8 texgen);
	S32 setShiny(U8 index, U8 shiny);
	S32 setFullbright(U8 index, U8 t);
	S32 setMediaFlags(U8 index, U8 media_flags);
	S32 setGlow(U8 index, F32 glow);
	S32 setMaterialID(U8 index, const LLMaterialID& matidp);
	S32 setMaterialParams(U8 index, const LLMaterialPtr paramsp);
	LLMaterialPtr getMaterialParams(U8 index);

	S32 size() const;

#if 0
	void forceResize(S32 new_size);
#endif
	void setSize(S32 new_size);

	void setAllIDs(const LLUUID& id);

private:
	// Private so that it cannot be used
	LLPrimTextureList(const LLPrimTextureList&)
	{
	}

protected:
	texture_list_t mEntryList;
};

#endif
