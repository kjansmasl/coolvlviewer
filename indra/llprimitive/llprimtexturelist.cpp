/**
 * @file llprimtexturelist.cpp
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

#include "linden_common.h"

#include "llprimtexturelist.h"

#include "llmaterialid.h"
#include "lltextureentry.h"

// static
//int (TMyClass::*pt2Member)(float, char, char) = NULL;                // C++
LLTextureEntry* (*LLPrimTextureList::sNewTextureEntryCallback)() = &(LLTextureEntry::newTextureEntry);

// static
void LLPrimTextureList::setNewTextureEntryCallback( LLTextureEntry* (*callback)() )
{
	if (callback)
	{
		LLPrimTextureList::sNewTextureEntryCallback = callback;
	}
	else
	{
		LLPrimTextureList::sNewTextureEntryCallback = &(LLTextureEntry::newTextureEntry);
	}
}

// static
// call this to get a new texture entry
LLTextureEntry* LLPrimTextureList::newTextureEntry()
{
	return (*sNewTextureEntryCallback)();
}

LLPrimTextureList::LLPrimTextureList()
{
}

// virtual
LLPrimTextureList::~LLPrimTextureList()
{
	clear();
}

void LLPrimTextureList::clear()
{
	texture_list_t::iterator itr = mEntryList.begin();
	while (itr != mEntryList.end())
	{
		delete *itr;
		*itr++ = NULL;
	}
	mEntryList.clear();
}

// Clears current entries, copies contents of other_list; this is somewhat
// expensive, so it must be called explicitly
void LLPrimTextureList::copy(const LLPrimTextureList& other_list)
{
	// Compare the sizes
	S32 this_size = mEntryList.size();
	S32 other_size = other_list.mEntryList.size();

	if (this_size > other_size)
	{
		// Remove the extra entries
		for (S32 index = this_size; index > other_size; --index)
		{
			delete mEntryList[index-1];
		}
		mEntryList.resize(other_size);
		this_size = other_size;
	}

	S32 index = 0;
	// Copy for the entries that already exist
	for ( ; index < this_size; ++index)
	{
		delete mEntryList[index];
		mEntryList[index] = other_list.getTexture(index)->newCopy();
	}

	// Add new entires if needed
	for ( ; index < other_size; ++index)
	{
		mEntryList.emplace_back(other_list.getTexture(index)->newCopy());
	}
}

// Clears current copies, takes contents of other_list, clears other_list
void LLPrimTextureList::take(LLPrimTextureList& other_list)
{
	clear();
	mEntryList = other_list.mEntryList;
	other_list.mEntryList.clear();
}

// Copies LLTextureEntry 'te' and returns TEM_CHANGE_TEXTURE if successful,
// otherwise TEM_CHANGE_NONE.
//virtual
S32 LLPrimTextureList::copyTexture(U8 index, const LLTextureEntry* te)
{
	if (index == 255)
	{
		llwarns << "ignore copy of invalid index (255)" << llendl;
		return TEM_CHANGE_NONE;
	}
	if (S32(index) >= mEntryList.size())
	{
		S32 current_size = mEntryList.size();
		llwarns << "ignore copy of index = " << S32(index)
				<< " into texture entry list of size = " << current_size
				<< llendl;
		return TEM_CHANGE_NONE;
	}

	// we're changing an existing entry
	llassert(mEntryList[index]);
	delete (mEntryList[index]);
	if  (te)
	{
		mEntryList[index] = te->newCopy();
	}
	else
	{
		mEntryList[index] = LLPrimTextureList::newTextureEntry();
	}
	return TEM_CHANGE_TEXTURE;
}

// Takes ownership of LLTextureEntry* 'te' and returns TEM_CHANGE_TEXTURE if
// successful, otherwise TEM_CHANGE_NONE. IMPORTANT: if you use this function
// you must check the return value.
//virtual
S32 LLPrimTextureList::takeTexture(U8 index, LLTextureEntry* te)
{
	if (index == 255 || S32(index) >= mEntryList.size())
	{
		return TEM_CHANGE_NONE;
	}

	// We are changing an existing entry
	llassert(mEntryList[index]);
	delete mEntryList[index];

	mEntryList[index] = te;

	return TEM_CHANGE_TEXTURE;
}

// returns pointer to texture at 'index' slot
LLTextureEntry* LLPrimTextureList::getTexture(U8 index) const
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index];
	}
	return NULL;
}

S32 LLPrimTextureList::setID(U8 index, const LLUUID& id)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setID(id);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setColor(U8 index, const LLColor3& color)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setColor(color);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setColor(U8 index, const LLColor4& color)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setColor(color);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setAlpha(U8 index, F32 alpha)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setAlpha(alpha);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setScale(U8 index, F32 s, F32 t)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setScale(s, t);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setScaleS(U8 index, F32 s)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setScaleS(s);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setScaleT(U8 index, F32 t)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setScaleT(t);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setOffset(U8 index, F32 s, F32 t)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setOffset(s, t);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setOffsetS(U8 index, F32 s)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setOffsetS(s);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setOffsetT(U8 index, F32 t)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setOffsetT(t);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setRotation(U8 index, F32 r)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setRotation(r);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setBumpShinyFullbright(U8 index, U8 bump)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setBumpShinyFullbright(bump);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setMediaTexGen(U8 index, U8 media)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setMediaTexGen(media);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setBumpMap(U8 index, U8 bump)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setBumpmap(bump);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setBumpShiny(U8 index, U8 bump_shiny)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setBumpShiny(bump_shiny);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setTexGen(U8 index, U8 texgen)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setTexGen(texgen);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setShiny(U8 index, U8 shiny)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setShiny(shiny);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setFullbright(U8 index, U8 fullbright)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setFullbright(fullbright);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setMediaFlags(U8 index, U8 media_flags)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setMediaFlags(media_flags);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setGlow(U8 index, F32 glow)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setGlow(glow);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setMaterialID(U8 index, const LLMaterialID& matidp)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setMaterialID(matidp);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimTextureList::setMaterialParams(U8 index, const LLMaterialPtr paramsp)
{
	if (index != 255 && index < mEntryList.size())
	{
		return mEntryList[index]->setMaterialParams(paramsp);
	}
	return TEM_CHANGE_NONE;
}

LLMaterialPtr LLPrimTextureList::getMaterialParams(U8 index)
{
	if (index < mEntryList.size())
	{
		return mEntryList[index]->getMaterialParams();
	}
	return LLMaterialPtr();
}

S32 LLPrimTextureList::size() const
{
	return mEntryList.size();
}

// Sets the size of the mEntryList container
void LLPrimTextureList::setSize(S32 new_size)
{
	if (new_size < 0)
	{
		new_size = 0;
	}

	S32 current_size = mEntryList.size();

	if (new_size > current_size)
	{
		mEntryList.resize(new_size);
		for (S32 index = current_size; index < new_size; ++index)
		{
			if (current_size > 0 && mEntryList[current_size - 1])
			{
				// Copy the last valid entry for the new one
				mEntryList[index] = mEntryList[current_size - 1]->newCopy();
			}
			else
			{
				// No valid enries to copy, so we new one up
				LLTextureEntry* new_entry =
					LLPrimTextureList::newTextureEntry();
				mEntryList[index] = new_entry;
			}
		}
	}
	else if (new_size < current_size)
	{
		for (S32 index = current_size - 1; index >= new_size; --index)
		{
			delete mEntryList[index];
		}
		mEntryList.resize(new_size);
	}
}

void LLPrimTextureList::setAllIDs(const LLUUID& id)
{
	texture_list_t::iterator itr = mEntryList.begin();
	while (itr != mEntryList.end())
	{
		(*itr++)->setID(id);
	}
}
