/**
 * @file llwearable.cpp
 * @brief LLWearable class implementation
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

#include "linden_common.h"

#include "llwearable.h"

#include "llavatarappearance.h"
#include "llavatarappearancedefines.h"
#include "lllocaltextureobject.h"
#include "lltexlayer.h"
#include "lltexturemanagerbridge.h"
#include "llvisualparam.h"

using namespace LLAvatarAppearanceDefines;

// Keep track of active wearables: LLWearableList needs this to avoid double-
// free !!!
fast_hset<LLWearable*> LLWearable::sWearableList;

//static
S32 LLWearable::sCurrentDefinitionVersion = 1;

// Private local helper function
static std::string terse_F32_to_string(F32 f)
{
	std::string r = llformat("%.2f", f);
	S32 len = r.length();

	// "1.20"  -> "1.2"
	// "24.00" -> "24."
	while (len > 0 && r[len - 1] == '0')
	{
		r.erase(--len, 1);
	}

	if (r[len - 1] == '.')
	{
		// "24." -> "24"
		r.erase(len - 1, 1);
	}
	else if (r[0] == '-' && r[1] == '0')
	{
		// "-0.59" -> "-.59"
		r.erase(1, 1);
	}
	else if (r[0] == '0')
	{
		// "0.59" -> ".59"
		r.erase(0, 1);
	}

	return r;
}

LLWearable::LLWearable()
:	mDefinitionVersion(-1),
	mName(),
	mDescription(),
	mPermissions(),
	mSaleInfo(),
	mType(LLWearableType::WT_NONE),
	mSavedVisualParamMap(),
	mVisualParamIndexMap(),
	mTEMap(),
	mSavedTEMap()
{
	sWearableList.insert(this);
}

//virtual
LLWearable::~LLWearable()
{
	sWearableList.erase(this);

	for (visual_param_index_map_t::iterator it = mVisualParamIndexMap.begin();
		 it != mVisualParamIndexMap.end(); ++it)
	{
		LLVisualParam* vp = it->second;
		if (vp)
		{
			vp->clearNextParam();
			delete vp;
			it->second = NULL;
		}
	}
	mVisualParamIndexMap.clear();

	destroyTextures();
}

const std::string& LLWearable::getTypeLabel() const
{
	return LLWearableType::getTypeLabel(mType);
}

const std::string& LLWearable::getTypeName() const
{
	return LLWearableType::getTypeName(mType);
}

LLAssetType::EType LLWearable::getAssetType() const
{
	return LLWearableType::getAssetType(mType);
}

bool LLWearable::exportFile(const std::string& filename) const
{
	llofstream ofs(filename.c_str(),
				   std::ios_base::out | std::ios_base::trunc |
				   std::ios_base::binary);
	return ofs.is_open() && exportStream(ofs);
}

//virtual
bool LLWearable::exportStream(std::ostream& output_stream) const
{
	if (!output_stream.good()) return false;

	// Header and version
	output_stream << "LLWearable version " << mDefinitionVersion << "\n";
	// Name
	output_stream << mName << "\n";
	// Description
	output_stream << mDescription << "\n";

	// Permissions
	if (!mPermissions.exportLegacyStream(output_stream))
	{
		return false;
	}

	// Sale info
	if (!mSaleInfo.exportLegacyStream(output_stream))
	{
		return false;
	}

	// Wearable type
	output_stream << "type " << (S32) getType() << "\n";

	// Parameters
	output_stream << "parameters " << mVisualParamIndexMap.size() << "\n";

	for (visual_param_index_map_t::const_iterator
			iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		S32 param_id = iter->first;
		const LLVisualParam* param = iter->second;
		F32 param_weight = param->getWeight();
		output_stream << param_id << " " << terse_F32_to_string(param_weight)
					  << "\n";
	}

	// Texture entries
	output_stream << "textures " << mTEMap.size() << "\n";

	for (te_map_t::const_iterator iter = mTEMap.begin(), end = mTEMap.end();
		 iter != end; ++iter)
	{
		S32 te = iter->first;
		const LLUUID& image_id = iter->second->getID();
		output_stream << te << " " << image_id << "\n";
	}
	return true;
}

void LLWearable::createVisualParams(LLAvatarAppearance* avatarp)
{
	for (LLViewerVisualParam* param =
			(LLViewerVisualParam*)avatarp->getFirstVisualParam();
		 param; param = (LLViewerVisualParam*) avatarp->getNextVisualParam())
	{
		if (param->getWearableType() == mType)
		{
			LLVisualParam* clone_param = param->cloneParam(this);
			clone_param->setParamLocation(LOC_UNKNOWN);
			clone_param->setParamLocation(LOC_WEARABLE);
			addVisualParam(clone_param);
		}
	}

	// Resync driver parameters to point to the newly cloned driven
	// parameters
	for (visual_param_index_map_t::iterator
			param_iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
		 param_iter != end; ++param_iter)
	{
		LLVisualParam* param = param_iter->second;
		LLVisualParam*(LLWearable::*wearable_function)(S32)const =
			&LLWearable::getVisualParam;
		// need this line to disambiguate between versions of
		// LLCharacter::getVisualParam()
		LLVisualParam*(LLAvatarAppearance::*param_function)(S32)const =
			&LLAvatarAppearance::getVisualParam;
		param->resetDrivenParams();
		if (!param->linkDrivenParams(boost::bind(wearable_function,
												 (LLWearable*)this, _1),
									 false))
		{
			if (!param->linkDrivenParams(boost::bind(param_function, avatarp,
													 _1),
										 true))
			{
#if 0			// Temporarily made into a debug message, till LL determines
				// whether non-linked params are a normal occurrence or not...
				llwarns << "Could not link driven params for wearable "
						<< getName() << " id: " << param->getID() << llendl;
#else
				LL_DEBUGS("Avatar") << "Could not link driven params for wearable "
									<< getName() << " id: " << param->getID()
									<< LL_ENDL;
#endif
				continue;
			}
		}
	}
}

void LLWearable::createLayers(S32 te, LLAvatarAppearance* avatarp)
{
	LLTexLayerSet* layer_set = NULL;
	const LLAvatarAppearanceDictionary::TextureEntry* texture_dict =
		gAvatarAppDictp->getTexture((ETextureIndex)te);
	if (texture_dict && texture_dict->mIsUsedByBakedTexture)
	{
		const EBakedTextureIndex baked_index =
			texture_dict->mBakedTextureIndex;
		layer_set = avatarp->getAvatarLayerSet(baked_index);
	}

	if (layer_set)
	{
		layer_set->cloneTemplates(mTEMap[te], (ETextureIndex)te, this);
	}
	else
	{
		llwarns << "Could not find layerset for texture entry " << te
				<< " in wearable. mIsUsedByBakedTexture = "
				<< texture_dict->mBakedTextureIndex << llendl;
	}
}

LLWearable::EImportResult LLWearable::importFile(const std::string& filename,
												 LLAvatarAppearance* avatarp)
{
	llifstream ifs(filename.c_str(), std::ios_base::in | std::ios_base::binary);
	return ifs.is_open() ? importStream(ifs, avatarp) : FAILURE;
}

//virtual
LLWearable::EImportResult LLWearable::importStream(std::istream& input_stream,
												   LLAvatarAppearance* avatarp)
{
	if (!avatarp || !gTextureManagerBridgep)
	{
		return FAILURE;
	}

	// *NOTE: changing the type or size of this buffer will require changes in
	// the fscanf() code below.
	// We are using a local max buffer size here to avoid issues if MAX_STRING
	// size changes.
	constexpr U32 PARSE_BUFFER_SIZE = 2048;
	char buffer[PARSE_BUFFER_SIZE];
	char uuid_buffer[37];

	// This data is being generated on the viewer.
	// Impose some sane limits on parameter and texture counts.
	constexpr S32 MAX_WEARABLE_ASSET_TEXTURES = 100;
	constexpr S32 MAX_WEARABLE_ASSET_PARAMETERS = 1000;

	// Read header and version
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Failed to read wearable asset input stream." << llendl;
		return FAILURE;
	}
	if (sscanf(buffer, "LLWearable version %d\n", &mDefinitionVersion) != 1)
	{
		return BAD_HEADER;
	}

	// Temporary hack to allow wearables with definition version 24 to still
	// load. This should only affect lindens and NDA'd testers who have saved
	// wearables in 2.0 the extra check for version == 24 can be removed before
	// release, once internal testers have loaded these wearables again.
	// See hack pt 2 at bottom of function to ensure that these wearables get
	// re-saved with version definition 22.
	if (mDefinitionVersion > sCurrentDefinitionVersion &&
		mDefinitionVersion != 24)
	{
		llwarns << "Wearable asset has newer version (" << mDefinitionVersion
				<< ") than XML (" << sCurrentDefinitionVersion << ")"
				<< llendl;
		return FAILURE;
	}

	// Name may be empty
	if (!input_stream.good())
	{
		llwarns << "Bad wearable asset: early end of input stream while reading name"
				<< llendl;
		return FAILURE;
	}
	input_stream.getline(buffer, PARSE_BUFFER_SIZE);
	mName = buffer;

	// Description may be empty
	if (!input_stream.good())
	{
		llwarns << "Bad wearable asset: early end of input stream while reading description"
				<< llendl;
		return FAILURE;
	}
	input_stream.getline(buffer, PARSE_BUFFER_SIZE);
	mDescription = buffer;

	// Permissions may have extra empty lines before the correct line
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Bad wearable asset: early end of input stream while reading permissions"
				<< llendl;
		return FAILURE;
	}
	S32 perm_version = -1;
	if (sscanf(buffer, " permissions %d\n", &perm_version) != 1 ||
		perm_version != 0)
	{
		llwarns << "Bad wearable asset: missing valid permissions" << llendl;
		return FAILURE;
	}
	if (!mPermissions.importLegacyStream(input_stream))
	{
		return FAILURE;
	}

	// Sale info
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Bad wearable asset: early end of input stream while reading sale info"
				<< llendl;
		return FAILURE;
	}
	S32 sale_info_version = -1;
	if (sscanf(buffer, " sale_info %d\n", &sale_info_version) != 1 ||
		sale_info_version != 0)
	{
		llwarns << "Bad wearable asset: missing valid sale_info" << llendl;
		return FAILURE;
	}
	// Sale info used to contain next owner perm. It is now in the permissions.
	// Thus, we read that out, and fix legacy objects. It's possible this op
	// would fail, but it should pick up the vast majority of the tasks.
	bool has_perm_mask = false;
	U32 perm_mask = 0;
	if (!mSaleInfo.importLegacyStream(input_stream, has_perm_mask, perm_mask))
	{
		return FAILURE;
	}
	if (has_perm_mask)
	{
		// Fair use fix.
		if (!(perm_mask & PERM_COPY))
		{
			perm_mask |= PERM_TRANSFER;
		}
		mPermissions.setMaskNext(perm_mask);
	}

	// Wearable type
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Bad wearable asset: early end of input stream while reading type"
				<< llendl;
		return FAILURE;
	}
	S32 type = -1;
	if (sscanf(buffer, "type %d\n", &type) != 1)
	{
		llwarns << "Bad wearable asset: cannot read type" << llendl;
		return FAILURE;
	}
	if (type >= 0 && type < LLWearableType::WT_COUNT)
	{
		setType((LLWearableType::EType)type, avatarp);
	}
	else
	{
		mType = LLWearableType::WT_COUNT;
		llwarns << "Bad wearable asset: bad type #" << type << llendl;
		return FAILURE;
	}

	// Parameters header
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Bad wearable asset: early end of input stream while reading parameters header. Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
		return FAILURE;
	}
	S32 num_parameters = -1;
	if (sscanf(buffer, "parameters %d\n", &num_parameters) != 1)
	{
		llwarns << "Bad wearable asset: missing parameters block. Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
		return FAILURE;
	}
	if (num_parameters > MAX_WEARABLE_ASSET_PARAMETERS)
	{
		llwarns << "Bad wearable asset: too many parameters: "
				<< num_parameters << ". Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
		return FAILURE;
	}
	S32 param_count = mVisualParamIndexMap.size();
	if (num_parameters > param_count)
	{
		llwarns << "Wearable parameter mismatch. Reading in "
				<< num_parameters << " from file, but created " << param_count
				<< " from avatar parameters. Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
	}
	else if (num_parameters < param_count)
	{
		llinfos << "Old wearable detected. Reading in "
				<< num_parameters << " from file, but created " << param_count
				<< " from avatar parameters. Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
	}

	// Parameters
	S32 i;
	for (i = 0; i < num_parameters; ++i)
	{
		if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
		{
			llwarns << "Bad wearable asset: early end of input stream "
					<< "while reading parameter #" << i << ". Type: "
					<< LLWearableType::getTypeName(getType()) << llendl;
			return FAILURE;
		}
		S32 param_id = 0;
		F32 param_weight = 0.f;
		if (sscanf(buffer, "%d %f\n", &param_id, &param_weight) != 2)
		{
			llwarns << "Bad wearable asset: bad parameter, #" << i
					<< ". Type: " << LLWearableType::getTypeName(getType())
					<< llendl;
			return FAILURE;
		}
		mSavedVisualParamMap[param_id] = param_weight;
	}

	// Textures header
	if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
	{
		llwarns << "Bad wearable asset: early end of input stream while reading textures header #"
				<< i << ". Type: " << LLWearableType::getTypeName(getType())
				<< llendl;
		return FAILURE;
	}
	S32 num_textures = -1;
	if (sscanf(buffer, "textures %d\n", &num_textures) != 1)
	{
		llwarns << "Bad wearable asset: missing textures block. Type: "
				<< LLWearableType::getTypeName(getType()) << llendl;
		return FAILURE;
	}
	if (num_textures > MAX_WEARABLE_ASSET_TEXTURES)
	{
		llwarns << "Bad wearable asset: too many textures: " << num_textures
				<< ". Type: " << LLWearableType::getTypeName(getType())
				<< llendl;
		return FAILURE;
	}

	// Textures
	for (i = 0; i < num_textures; ++i)
	{
		if (!getNextPopulatedLine(input_stream, buffer, PARSE_BUFFER_SIZE))
		{
			llwarns << "Bad wearable asset: early end of input stream "
					<< "while reading textures #" << i << ". Type: "
					<< LLWearableType::getTypeName(getType()) << llendl;
			return FAILURE;
		}
		S32 te = 0;
		if (sscanf(buffer, "%d %36s\n", &te, uuid_buffer) != 2)
		{
			llwarns << "Bad wearable asset: bad texture, #" << i << ". Type: "
					<< LLWearableType::getTypeName(getType()) << llendl;
			return FAILURE;
		}
		if (te >= TEX_NUM_INDICES)
		{
			llwarns << "Bad wearable asset: texture index too large " << te
					<< ". Type: " << LLWearableType::getTypeName(getType())
					<< llendl;
			return FAILURE;
		}
		if (!LLUUID::validate(uuid_buffer))
		{
			llwarns << "Bad wearable asset: bad texture uuid: " << uuid_buffer
					<< ". Type: " << LLWearableType::getTypeName(getType())
					<< llendl;
			return FAILURE;
		}
		LLUUID id = LLUUID(uuid_buffer);
		LLGLTexture* image = gTextureManagerBridgep->getFetchedTexture(id);
		if (mTEMap.find(te) != mTEMap.end())
		{
			delete mTEMap[te];
		}
		if (mSavedTEMap.find(te) != mSavedTEMap.end())
		{
			delete mSavedTEMap[te];
		}

		LLUUID textureid(uuid_buffer);
		mTEMap[te] = new LLLocalTextureObject(image, textureid);
		mSavedTEMap[te] = new LLLocalTextureObject(image, textureid);
		createLayers(te, avatarp);
	}

	// Copy all saved param values to working params
	revertValues();

	return SUCCESS;
}

bool LLWearable::getNextPopulatedLine(std::istream& input_stream, char* buffer,
									  U32 buffer_size)
{
	if (!input_stream.good())
	{
		return false;
	}

	do
	{
		input_stream.getline(buffer, buffer_size);
	}
	while (input_stream.good() && buffer[0] == '\0');

	return buffer[0] != '\0';
}

void LLWearable::setType(LLWearableType::EType type,
						 LLAvatarAppearance* avatarp)
{
	mType = type;
	createVisualParams(avatarp);
}

LLLocalTextureObject* LLWearable::getLocalTextureObject(S32 index)
{
	te_map_t::iterator iter = mTEMap.find(index);
	if (iter != mTEMap.end())
	{
		LLLocalTextureObject* lto = iter->second;
		return lto;
	}
	return NULL;
}

const LLLocalTextureObject* LLWearable::getLocalTextureObject(S32 index) const
{
	te_map_t::const_iterator iter = mTEMap.find(index);
	if (iter != mTEMap.end())
	{
		const LLLocalTextureObject* lto = iter->second;
		return lto;
	}
	return NULL;
}

void LLWearable::getLocalTextureListSeq(std::vector<LLLocalTextureObject*>& lv)
{
	lv.clear();
	lv.reserve(mTEMap.size());
	for (te_map_t::const_iterator iter = mTEMap.begin(), end = mTEMap.end();
		 iter != end; ++iter)
	{
		LLLocalTextureObject* ltop = iter->second;
		if (ltop)
		{
			lv.push_back(ltop);
		}
	}
}

LLLocalTextureObject* LLWearable::setLocalTextureObject(S32 index,
														LLLocalTextureObject& lto)
{
	if (mTEMap.find(index) != mTEMap.end())
	{
		mTEMap.erase(index);
	}
	LLLocalTextureObject* clone_lto = new LLLocalTextureObject(lto);
	mTEMap[index] = clone_lto;
	return clone_lto;
}

// *FIXME: this triggers changes to driven params on avatar, potentially
// clobbering baked appearance.
void LLWearable::revertValues()
{
	// Update saved settings so wearable is no longer dirty; non-driver params
	// first.
	for (param_map_t::const_iterator iter = mSavedVisualParamMap.begin(),
									 end = mSavedVisualParamMap.end();
		 iter != end; ++iter)
	{
		S32 id = iter->first;
		F32 value = iter->second;
		LLVisualParam* param = getVisualParam(id);
		if (param && !param->asDriverParam())
		{
			setVisualParamWeight(id, value, true);
		}
	}

	// Then update driver params
	for (param_map_t::const_iterator iter = mSavedVisualParamMap.begin(),
									 end = mSavedVisualParamMap.end();
		 iter != end; ++iter)
	{
		S32 id = iter->first;
		F32 value = iter->second;
		LLVisualParam* param = getVisualParam(id);
		if (param && param->asDriverParam())
		{
			setVisualParamWeight(id, value, true);
		}
	}

	// Make sure that saved values are sane
	for (param_map_t::const_iterator iter = mSavedVisualParamMap.begin(),
									 end = mSavedVisualParamMap.end();
		 iter != end; ++iter)
	{
		S32 id = iter->first;
		LLVisualParam* param = getVisualParam(id);
		if (param)
		{
			mSavedVisualParamMap[id] = param->getWeight();
		}
	}

	syncImages(mSavedTEMap, mTEMap);
}

void LLWearable::saveValues()
{
	// Update saved settings so wearable is no longer dirty
	mSavedVisualParamMap.clear();
	for (visual_param_index_map_t::const_iterator
			iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		S32 id = iter->first;
		LLVisualParam* wearable_param = iter->second;
		F32 value = wearable_param->getWeight();
		mSavedVisualParamMap[id] = value;
	}

	// Deep copy of mTEMap (copies only those tes that are current, filling in
	// defaults where needed)
	syncImages(mTEMap, mSavedTEMap);
}

void LLWearable::syncImages(te_map_t& src, te_map_t& dst)
{
	// Deep copy of mTEMap (copies only those tes that are current, filling in
	// defaults where needed)
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			te_map_t::const_iterator iter = src.find(te);
			LLUUID image_id;
			LLGLTexture* image = NULL;
			LLLocalTextureObject* lto = NULL;
			if (iter != src.end())
			{
				// There is a Local Texture Object in the source image map. Use
				// this to populate the values to store in the destination
				// image map.
				lto = iter->second;
				image = lto->getImage();
				image_id = lto->getID();
			}
			else
			{
				// There is no Local Texture Object in the source image map.
				// Get the defaults values for populating the destination image
				// map.
				image_id = getDefaultTextureImageID((ETextureIndex) te);
				if (gTextureManagerBridgep)
				{
					image = gTextureManagerBridgep->getFetchedTexture(image_id);
				}
			}

			if (dst.find(te) != dst.end())
			{
				// There is already an entry in the destination map for the
				// texture. Just update its values.
				dst[te]->setImage(image);
				dst[te]->setID(image_id);
			}
			else
			{
				// No entry found in the destination map, we need to create a
				// new Local Texture Object
				dst[te] = new LLLocalTextureObject(image, image_id);
			}

			if (lto)
			{
				// If we pulled values from a Local Texture Object in the
				// source map, make sure the proper flags are set in the new
				// (or updated) entry in the destination map.
				dst[te]->setBakedReady(lto->getBakedReady());
				dst[te]->setDiscard(lto->getDiscard());
			}
		}
	}
}

void LLWearable::destroyTextures()
{
	std::for_each(mTEMap.begin(), mTEMap.end(), DeletePairedPointer());
	mTEMap.clear();

	std::for_each(mSavedTEMap.begin(), mSavedTEMap.end(),
				  DeletePairedPointer());
	mSavedTEMap.clear();
}

void LLWearable::addVisualParam(LLVisualParam* param)
{
	S32 id = param->getID();
	if (mVisualParamIndexMap[id])
	{
		delete mVisualParamIndexMap[id];
	}
	param->setIsDummy(false);
	param->setParamLocation(LOC_WEARABLE);
	mVisualParamIndexMap[id] = param;
	mSavedVisualParamMap[id] = param->getDefaultWeight();
}

void LLWearable::setVisualParamWeight(S32 i, F32 value, bool upload_bake)
{
	visual_param_index_map_t::iterator it = mVisualParamIndexMap.find(i);
	if (it == mVisualParamIndexMap.end())
	{
		llwarns << "Passed invalid parameter index #" << i << " for wearable: "
				<< getName() << llendl;
	}
	it->second->setWeight(value, upload_bake);
}

F32 LLWearable::getVisualParamWeight(S32 i) const
{
	visual_param_index_map_t::const_iterator it = mVisualParamIndexMap.find(i);
	if (it == mVisualParamIndexMap.end())
	{
		llwarns << "Passed invalid parameter index #" << i << " for wearable: "
				<< getName() << llendl;
		return -1.f;
	}
	const LLVisualParam* wearable_param = it->second;
	return wearable_param->getWeight();
}

LLVisualParam* LLWearable::getVisualParam(S32 i) const
{
	visual_param_index_map_t::const_iterator it = mVisualParamIndexMap.find(i);
	return it == mVisualParamIndexMap.end() ? NULL : it->second;
}

void LLWearable::getVisualParams(visual_param_vec_t& list)
{
	list.reserve(mVisualParamIndexMap.size());
	// Add all visual params to the passed-in vector
	for (visual_param_index_map_t::iterator
			iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		list.push_back(iter->second);
	}
}

void LLWearable::animateParams(F32 delta, bool upload_bake)
{
	for (visual_param_index_map_t::iterator
			iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		LLVisualParam* param = (LLVisualParam*)iter->second;
		param->animate(delta, upload_bake);
	}
}

LLColor4 LLWearable::getClothesColor(S32 te) const
{
	LLColor4 color;
	U32 param_name[3];
	if (LLAvatarAppearance::teToColorParams((LLAvatarAppearanceDefines::ETextureIndex)te,
											param_name))
	{
		for (U8 index = 0; index < 3; ++index)
		{
			color.mV[index] = getVisualParamWeight(param_name[index]);
		}
	}
	return color;
}

void LLWearable::setClothesColor(S32 te, const LLColor4& new_color,
								 bool upload_bake)
{
	U32 param_name[3];
	if (LLAvatarAppearance::teToColorParams((LLAvatarAppearanceDefines::ETextureIndex)te,
											param_name))
	{
		for (U8 index = 0; index < 3; ++index)
		{
			setVisualParamWeight(param_name[index], new_color.mV[index],
								 upload_bake);
		}
	}
}

void LLWearable::writeToAvatar(LLAvatarAppearance* avatarp)
{
	if (!avatarp) return;

	// Pull params
	for (LLVisualParam* param = avatarp->getFirstVisualParam(); param;
		 param = avatarp->getNextVisualParam())
	{
		if (param &&
			((LLViewerVisualParam*)param)->getWearableType() == mType &&
			 // Cross-wearable parameters are not authoritative, as they are
			 // driven by a different wearable. So do not copy the values to
			 // the avatar object if cross wearable. Cross wearable params get
			 // their values from the avatar, they shouldn't write the other
			 // way.
			 !((LLViewerVisualParam*)param)->getCrossWearable())
		{
			S32 param_id = param->getID();
			F32 weight = getVisualParamWeight(param_id);
			avatarp->setVisualParamWeight(param_id, weight, false);
		}
	}
}
