/**
 * @file hbobjectbackup.cpp
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Original implementation Copyright (c) 2008 Merkat viewer authors.
 * Debugged/rewritten/augmented code Copyright (c) 2008-2023 Henri Beauchamp.
 *
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

#include "hbobjectbackup.h"

#include "imageids.h"
#include "llalertdialog.h"
#include "llcallbacklist.h"
#include "lldir.h"
#include "lleconomy.h"
#include "llfilesystem.h"
#include "llimagej2c.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloaterperms.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llinventorymodel.h"			// For gInventory
#include "llmaterialmgr.h"
#include "lltexturecache.h"
#include "lltoolplacer.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"			// For create_inventory_item()
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "llviewermenu.h"

// Note: these default textures are initialized with hard coded values to
// prevent cheating. When not in SL, the user-configurable values are used
// instead (see setDefaultTextures() below).
LLUUID gTexturePlywood	   = IMG_PLYWOOD;
LLUUID gTextureBlank	   = IMG_BLANK;
LLUUID gTextureInvisible   = LLUUID("38b86f85-2575-52a9-a531-23108d8da837");
LLUUID gTextureTransparent = LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903");
LLUUID gTextureMedia	   = LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361");

//static
HBObjectBackup::rebase_map_t HBObjectBackup::sAssetMap;

class BackupCacheReadResponder final : public LLTextureCache::ReadResponder
{
protected:
	LOG_CLASS(BackupCacheReadResponder);

public:
	BackupCacheReadResponder(const LLUUID& id, LLImageFormatted* image)
	:	mFormattedImage(image), mID(id)
	{
		setImage(image);
	}

	void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat,
				 bool imagelocal) override
	{
		HBObjectBackup* self = HBObjectBackup::findInstance();
		if (!self) return;

		if (imageformat == IMG_CODEC_TGA &&
			mFormattedImage->getCodec() == IMG_CODEC_J2C)
		{
			llwarns << "FAILED: texture " << mID
					<< " is formatted as TGA. Not saving." << llendl;
			self->mNonExportedTextures |= HBObjectBackup::TEXTURE_BAD_ENCODING;
			mFormattedImage = NULL;
			mImageSize = 0;
			return;
		}

		if (mFormattedImage.notNull())
		{
			if (mFormattedImage->getCodec() == imageformat)
			{
				mFormattedImage->appendData(data, datasize);
			}
			else
			{
				llwarns << "FAILED: texture " << mID
						<< " is formatted as " << mFormattedImage->getCodec()
						<< " while expecting " << imageformat
						<< ". Not saving." << llendl;
				mFormattedImage = NULL;
				mImageSize = 0;
				return;
			}
		}
		else
		{
			mFormattedImage = LLImageFormatted::createFromType(imageformat);
			mFormattedImage->setData(data, datasize);
		}
		mImageSize = imagesize;
		mImageLocal = imagelocal;
	}

	void started() override
	{
	}

	void completed(bool success) override
	{
		HBObjectBackup* self = HBObjectBackup::findInstance();
		if (!self)
		{
			llwarns << "Export aborted, HBObjectBackup instance gone !"
					<< llendl;
			return;
		}

		if (success && mFormattedImage.notNull() && mImageSize > 0)
		{
			llinfos << "SUCCESS getting texture " << mID << llendl;
			std::string name;
			mID.toString(name);
			name = self->getFolder() + name;
			llinfos << "Saving to " << name << llendl;
			if (!mFormattedImage->save(name))
			{
				llwarns << "FAILED to save texture " << mID << llendl;
				self->mNonExportedTextures |= HBObjectBackup::TEXTURE_SAVED_FAILED;
			}
		}
		else
		{
			if (!success)
			{
				llwarns << "FAILED to get texture " << mID << llendl;
				self->mNonExportedTextures |= HBObjectBackup::TEXTURE_MISSING;
			}
			if (mFormattedImage.isNull())
			{
				llwarns << "FAILED: NULL texture " << mID << llendl;
				self->mNonExportedTextures |= HBObjectBackup::TEXTURE_IS_NULL;
			}
		}

		self->mCheckNextTexture = true;
	}

private:
	LLPointer<LLImageFormatted> mFormattedImage;
	LLUUID mID;
};

HBObjectBackup::HBObjectBackup(const LLSD&)
:	mRunning(false),
	mRetexture(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_object_backup.xml",
												 NULL, false);	// Do not open
}

//virtual
HBObjectBackup::~HBObjectBackup()
{
	// Just in case we got closed unexpectedly...
	gIdleCallbacks.deleteFunction(exportWorker);

	// Release the selection handle
	mSelectedObjects = NULL;
}

//static
bool HBObjectBackup::confirmCloseCallback(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		HBObjectBackup* self = findInstance();
		if (self)
		{
			self->destroy();
		}
	}
	return false;
}

//virtual
void HBObjectBackup::onClose(bool app_quitting)
{
	// Do not destroy the floater on user close action to avoid getting things
	// messed up during import/export.
	if (app_quitting)
	{
		destroy();
	}
	else
	{
		gNotifications.add("ConfirmAbortBackup", LLSD(), LLSD(),
						   confirmCloseCallback);
	}
}

void HBObjectBackup::showFloater(bool exporting)
{
	// Set the title
	setTitle(getString(exporting ? "export" : "import"));

	mCurObject = 1;
	mCurPrim = mObjects = mPrims = mRezCount = 0;

	// Make the floater pop up
	setVisibleAndFrontmost();
}

void HBObjectBackup::updateExportNumbers()
{
	std::stringstream sstr;
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");

	sstr << "Export Progress \n";

	sstr << "Remaining Textures " << mTexturesList.size() << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());
}

void HBObjectBackup::updateImportNumbers()
{
	std::stringstream sstr;
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");

	if (mRetexture)
	{
		sstr << " Textures uploads remaining : " << mTexturesList.size()
			 << "\n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}
	else
	{
		sstr << " Textures uploads N/A \n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}

	sstr << " Objects " << mCurObject << "/" << mObjects << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Rez " << mRezCount << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Build " << mCurPrim << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());
}

//static
void HBObjectBackup::exportCallback(HBFileSelector::ESaveFilter type,
									std::string& filename, void*)
{
	if (!filename.empty())
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object || object->isDead())
		{
			gNotifications.add("ExportAborted");
			return;
		}
		HBObjectBackup* self = getInstance();
		if (self)
		{
			self->doExportObject(filename);
		}
	}
}

//static
void HBObjectBackup::exportObject()
{
	if (findInstance())
	{
		llwarns << "Backup operation already in progress !" << llendl;
		showInstance();
	}
	else
	{
		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		if (selection && selection->getFirstRootNode())
		{
			std::string suggestion;
			suggestion = selection->getFirstRootNode()->mName;
			suggestion = LLDir::getScrubbedFileName(suggestion) + ".xml";
			// Open the file save dialog
			HBFileSelector::saveFile(HBFileSelector::FFSAVE_XML, suggestion,
									 exportCallback);
		}
	}
}

void HBObjectBackup::doExportObject(std::string filename)
{
	mFileName = filename;
	mFolder = gDirUtilp->getDirName(mFileName) + LL_DIR_DELIM_STR;
	mTexturesList.clear();
	mBadPermsTexturesList.clear();
	mLLSD.clear();
	mThisGroup.clear();
	setDefaultTextures();
	mNonExportedTextures = TEXTURE_OK;
	mExportState = EXPORT_INIT;
	mSelectedObjects = gSelectMgr.getSelection();
	mGotExtraPhysics = gAgent.hasRegionCapability("GetObjectPhysicsData");
	gIdleCallbacks.addFunction(exportWorker);
}

//---------------------------------------------------------------------------//
// Permissions checking functions
//---------------------------------------------------------------------------//

//static
void HBObjectBackup::setDefaultTextures()
{
	// When in SL or in an OpenSIM grid without export permission support, we
	// need to check for texture permissions based on their owner and UUID, and
	// we cannot trust settings that the user could have modified to try and
	// make a non-exportable texture pass for a default grid textures.
	if (gIsInSecondLife || gAgent.regionHasExportPermSupport())
	{
		return;
	}
	gTexturePlywood = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
	gTextureBlank = LLUUID(gSavedSettings.getString("UIImgWhiteUUID"));
	gTextureInvisible =
		LLUUID(gSavedSettings.getString("UIImgInvisibleUUID"));
}

//static
bool HBObjectBackup::validatePerms(const LLPermissions* item_permissions,
								   bool strict)
{
	// Let's see if we have got export permission bit support
	bool has_export_perm = gAgent.regionHasExportPermSupport();

	// By default, allow to export if the asset is full perm and owned by you.
	ExportPolicy policy = ep_full_perm;
	if (gIsInSecondLife || (strict && !has_export_perm))
	{
		// In Second Life you must be the creator to be permitted to export the
		// asset. This is also the case when asking for strict checking (i.e.
		// for mesh exporting or wearable textures viewing) in OpenSIM grids
		// without support for the export permission bit.
		policy = ep_creator_only;
	}
	else if (has_export_perm)
	{
		// In OpenSIM grids with support for export permission, rely on that
		// bit to allow/disallow the export.
		policy = ep_export_bit;
	}

	return item_permissions->allowExportBy(gAgentID, policy);
}

// So far, only Second Life forces TPVs to verify the creator for textures...
// which sucks, because there is no other way to check for a texture
// permissions or creator than to try and find an inventory item with the
// asset Id corresponding to the texture Id, and check the permissions or
// creator on the said inventory item, meaning that if you created the texture
// and subsequently deleted it from your inventory, you will not be able to
// export it any more !!!
// The "must be creator" stuff also goes against the usage in Linden Lab's own
// official viewers, since those allow you to save full perm textures (such as
// the textures in the Library), whoever is the actual creator... Go figure !

//static
bool HBObjectBackup::validateAssetPerms(const LLUUID& asset_id, bool strict)
{
	if (!strict && !gIsInSecondLife)
	{
		if (!gAgent.regionHasExportPermSupport())
		{
			// If we are not in Second Life, and this is not a strict checking
			// (used for wearables textures) and we do not have support for the
			// export permission, do not bother and unconditionally accept the
			// texture export (legacy behaviour).
			return true;
		}
	}

	if (asset_id == gTexturePlywood || asset_id == gTextureBlank ||
		asset_id == gTextureInvisible || asset_id == gTextureTransparent ||
		asset_id == gTextureMedia || asset_id == IMG_DEFAULT)
	{
		// Allow to export a few default SL textures.
		return true;
	}

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null, cats, items,
									LLInventoryModel::INCLUDE_TRASH,
									asset_id_matches);
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		const LLPermissions item_permissions = items[i]->getPermissions();
		if (validatePerms(&item_permissions, strict))
		{
			return true;
		}
	}

	return false;
}

LLUUID HBObjectBackup::validateTextureID(const LLUUID& asset_id)
{
	if (mBadPermsTexturesList.count(asset_id))
	{
		// We already checked it and know it is bad...
		return gTexturePlywood;
	}
	if (asset_id.isNull() || validateAssetPerms(asset_id))
	{
		return asset_id;
	}
	mBadPermsTexturesList.emplace(asset_id);	// Cache bad texture Id
	mNonExportedTextures |= TEXTURE_BAD_PERM;
	llwarns << "Bad permissions for texture Id: " << asset_id
			<< " - Texture will not be exported." << llendl;
	return gTexturePlywood;
}

//static
bool HBObjectBackup::validateNode(LLSelectNode* node)
{
	LLPermissions* perms = node->mPermissions;
	if (!perms || !validatePerms(perms))
	{
		return false;
	}

	// Additionally check if this is a sculpt or a mesh object and if yes, if
	// we have export permission on the sclupt texture or the mesh object.
	LLViewerObject* obj = node->getObject();
	if (!obj)	// Paranoia
	{
		return false;
	}

	if (obj->isSculpted())
	{
		if (obj->isMesh())
		{
			return validatePerms(perms, true);
		}

		const LLSculptParams* params = obj->getSculptParams();
		if (params)
		{
			const LLUUID& sculpt_id = params->getSculptTexture();
			return validateAssetPerms(sculpt_id);
		}
	}

	return true;
}

//---------------------------------------------------------------------------//

//static
void HBObjectBackup::exportWorker(void*)
{
	HBObjectBackup* self = findInstance();
	if (!self)
	{
		gIdleCallbacks.deleteFunction(exportWorker);
		llwarns << "Export process aborted. HBObjectBackup instance gone !"
				<< llendl;
		gNotifications.add("ExportAborted");
		return;
	}

	self->updateExportNumbers();

	switch (self->mExportState)
	{
		case EXPORT_INIT:
		{
			self->showFloater(true);
			// Fall through to EXPORT_CHECK_PERMS
		}
		case EXPORT_CHECK_PERMS:
		{
			struct ff final : public LLSelectedNodeFunctor
			{
				bool apply(LLSelectNode* node) override
				{
					return HBObjectBackup::validateNode(node);
				}
			} func;

			LLViewerObject* object;
			object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				if (self->mSelectedObjects->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_FETCH_PHYSICS;
				}
				else
				{
					struct vv final : public LLSelectedNodeFunctor
					{
						bool apply(LLSelectNode* node) override
						{
							return node->mValid;
						}
					} func2;

					if (self->mSelectedObjects->applyToNodes(&func2, false))
					{
						llwarns << "Incorrect permission to export" << llendl;
						self->mExportState = EXPORT_FAILED;
					}
					else
					{
						LL_DEBUGS("ObjectBackup") << "Nodes permissions not yet received, delaying..."
												  << LL_ENDL;
						self->mExportState = EXPORT_CHECK_PERMS;
					}
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_FETCH_PHYSICS:
		{
			// Do not bother to try and fetch the extra physics flags if we
			// do not have sim support for them...
			if (!self->mGotExtraPhysics)
			{
				self->mExportState = EXPORT_STRUCTURE;
				break;
			}

			struct ff final : public LLSelectedNodeFunctor
			{
				bool apply(LLSelectNode* node) override
				{
					LLViewerObject* object = node->getObject();
					return gObjectList.gotObjectPhysicsFlags(object);
				}
			} func;

			LLViewerObject* object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				if (self->mSelectedObjects->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_STRUCTURE;
				}
				else
				{
					LL_DEBUGS("ObjectBackup") << "Nodes physics not yet received, delaying..."
											  << LL_ENDL;
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_STRUCTURE:
		{
			struct ff final : public LLSelectedObjectFunctor
			{
				bool apply(LLViewerObject* object) override
				{
					bool is_attachment = object->isAttachment();
					object->boostTexturePriority(true);
					LLViewerObject::child_list_t children =
						object->getChildren();
					children.push_front(object); // Push root onto list
					HBObjectBackup* self = findInstance();
					LLSD prim_llsd = self->primsToLLSD(children,
													   is_attachment);
					LLSD stuff;
					if (is_attachment)
					{
						stuff["root_position"] =
							object->getPositionEdit().getValue();
						stuff["root_rotation"] =
							ll_sd_from_quaternion(object->getRotationEdit());
					}
					else
					{
						stuff["root_position"] =
							object->getPosition().getValue();
						stuff["root_rotation"] =
							ll_sd_from_quaternion(object->getRotation());
					}
					stuff["group_body"] = prim_llsd;
					self->mLLSD["data"].append(stuff);
					return true;
				}
			} func;

			LLViewerObject* object;
			object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				self->mExportState = EXPORT_LLSD;
				self->mSelectedObjects->applyToRootObjects(&func, false);
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_TEXTURES:
		{
			if (!self->mCheckNextTexture)
			{
				// The texture is being fetched. Wait till next idle callback.
				return;
			}

			if (self->mTexturesList.empty())
			{
				self->mExportState = EXPORT_DONE;
				return;
			}

			// Ok, we got work to do...
			self->mCheckNextTexture = false;
			self->exportNextTexture();
			break;
		}

		case EXPORT_LLSD:
		{
			// Create a file stream and write to it
			llofstream export_file(self->mFileName.c_str());
			if (export_file.is_open())
			{
				LLSDSerialize::toPrettyXML(self->mLLSD, export_file);
				export_file.close();
				self->mCheckNextTexture = true;
				self->mExportState = EXPORT_TEXTURES;
			}
			else
			{
				llwarns << "Could not open file '" << self->mFileName
						<< "' for writing." << llendl;
				self->mExportState = EXPORT_FAILED;
			}
			break;
		}

		case EXPORT_DONE:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			if (self->mNonExportedTextures == HBObjectBackup::TEXTURE_OK)
			{
				llinfos << "Export successful and complete." << llendl;
				gNotifications.add("ExportSuccessful");
			}
			else
			{
				llinfos << "Export successful but incomplete: some texture(s) not saved."
						<< llendl;
				// *TODO: translate
				std::string reason;
				U32 error_bits_map = self->mNonExportedTextures;
				if (error_bits_map & HBObjectBackup::TEXTURE_BAD_PERM)
				{
					reason += "\nBad permissions/creator.";
				}
				if (error_bits_map & HBObjectBackup::TEXTURE_MISSING)
				{
					reason += "\nMissing texture (retrying after full rezzing might work).";
				}
				if (error_bits_map & HBObjectBackup::TEXTURE_BAD_ENCODING)
				{
					reason += "\nBad texture encoding.";
				}
				if (error_bits_map & HBObjectBackup::TEXTURE_IS_NULL)
				{
					reason += "\nNull texture.";
				}
				if (error_bits_map & HBObjectBackup::TEXTURE_SAVED_FAILED)
				{
					reason += "\nCould not write to disk.";
				}
				LLSD args;
				args["REASON"] = reason;
				gNotifications.add("ExportPartial", args);
			}
			self->destroy();
			break;
		}

		case EXPORT_FAILED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			llwarns << "Export process failed." << llendl;
			gNotifications.add("ExportFailed");
			self->destroy();
			break;
		}

		case EXPORT_ABORTED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			llwarns << "Export process aborted." << llendl;
			gNotifications.add("ExportAborted");
			self->destroy();
			break;
		}
	}
}

LLSD HBObjectBackup::primsToLLSD(LLViewerObject::child_list_t child_list,
								 bool is_attachment)
{
	LLSD llsd;
	char localid[16];
	LLUUID t_id;

	for (LLViewerObject::child_list_t::iterator i = child_list.begin();
		 i != child_list.end(); ++i)
	{
		LLViewerObject* objectp = i->get();
		if (!objectp || objectp->isDead())
		{
			continue;
		}

		llinfos << "Exporting prim " << objectp->getID() << llendl;

		// Create an LLSD object that represents this prim. It will be injected
		// in to the overall LLSD tree structure
		LLSD prim_llsd;

		if (!objectp->isRoot())
		{
			// Parent id
			snprintf(localid, sizeof(localid), "%u",
					 objectp->getSubParent()->getLocalID());
			prim_llsd["parent"] = localid;
		}

		// Name and description
		LLSelectNode* node = mSelectedObjects->findNode(objectp);
		if (node)
		{
			prim_llsd["name"] = node->mName;
			prim_llsd["description"] = node->mDescription;
		}

		// Transforms
		if (is_attachment)
		{
			prim_llsd["position"] = objectp->getPositionEdit().getValue();
			prim_llsd["rotation"] =
				ll_sd_from_quaternion(objectp->getRotationEdit());
		}
		else
		{
			prim_llsd["position"] = objectp->getPosition().getValue();
			prim_llsd["rotation"] =
				ll_sd_from_quaternion(objectp->getRotation());
		}
		prim_llsd["scale"] = objectp->getScale().getValue();

		// Flags
		prim_llsd["phantom"] = objectp->flagPhantom();		// Legacy
		prim_llsd["physical"] = objectp->flagUsePhysics();	// Legacy
		prim_llsd["flags"] = (S32)objectp->getFlags();		// New way

		// Extra physics flags
		if (mGotExtraPhysics)
		{
			LLSD& physics = prim_llsd["ExtraPhysics"];
			physics["PhysicsShapeType"] = objectp->getPhysicsShapeType();
			physics["Gravity"] = objectp->getPhysicsGravity();
			physics["Friction"] = objectp->getPhysicsFriction();
			physics["Density"] = objectp->getPhysicsDensity();
			physics["Restitution"] = objectp->getPhysicsRestitution();
		}

		// Click action
		prim_llsd["clickaction"] = objectp->getClickAction();

		// Prim "material" type (wood, metal, rubber, etc)
		prim_llsd["material"] = objectp->getMaterial();

		// Volume params
		LLVolumeParams params = objectp->getVolume()->getParams();
		prim_llsd["volume"] = params.asLLSD();

		// Extra params

		// Flexible
		if (objectp->isFlexible())
		{
			const LLFlexibleObjectData* datap =
				objectp->getFlexibleObjectData();
			if (datap)
			{
				prim_llsd["flexible"] = datap->asLLSD();
			}
		}

		// Light
		const LLLightParams* light_paramsp = objectp->getLightParams();
		if (light_paramsp)
		{
			prim_llsd["light"] = light_paramsp->asLLSD();
		}

		// Light image
		const LLLightImageParams* light_imgp = objectp->getLightImageParams();
		if (light_imgp)
		{
			t_id = validateTextureID(light_imgp->getLightTexture());
			if (t_id.notNull() && !mTexturesList.count(t_id))
			{
				llinfos << "Found a light texture, adding to list " << t_id
						<< llendl;
				mTexturesList.emplace(t_id);
			}
			prim_llsd["light_texture"] = light_imgp->asLLSD();
		}

		// Sculpt
		const LLSculptParams* sculptp = objectp->getSculptParams();
		if (sculptp)
		{
			prim_llsd["sculpt"] = sculptp->asLLSD();
			if ((sculptp->getSculptType() & LL_SCULPT_TYPE_MASK) !=
					LL_SCULPT_TYPE_MESH)
			{
				t_id = sculptp->getSculptTexture();
				if (t_id.notNull() && t_id == validateTextureID(t_id))
				{
					if (!mTexturesList.count(t_id))
					{
						llinfos << "Found a sculpt texture, adding to list "
								<< t_id << llendl;
						mTexturesList.emplace(t_id);
					}
				}
				else
				{
					llwarns << "Incorrect permission to export a sculpt texture."
							<< llendl;
					mExportState = EXPORT_FAILED;
				}
			}
		}

		// Textures and materials
		LLSD te_llsd;
		LLSD this_te_llsd;
		LLSD te_mat_llsd;
		LLSD te_gltf_llsd;
		LLSD this_te_mat_llsd;
		bool has_materials = false;
		bool has_pbr_mats = false;
		for (U8 i = 0, count = objectp->getNumTEs(); i < count; ++i)
		{
			LLTextureEntry* tep = objectp->getTE(i);
			if (!tep) continue;	// Paranoia

			// Diffuse map
			t_id = validateTextureID(tep->getID());
			this_te_llsd = tep->asLLSD();
			// Note: LL's code adds a "gltf_override" entry to the LLSD
			// returned by LLTextureEntry:asLLSD() when a PBR material
			// is set for that face, unlike what happens for legacy materials
			// (which are not dumped). We, however, store PBR material in their
			// own a "gltf_materials" array, to stay in line with what happens
			// for legacy materials. So, just remove this "noise"...
			this_te_llsd.erase("gltf_override");
			this_te_llsd["imageid"] = t_id;
			te_llsd.append(this_te_llsd);
			// Do not export non-existent default textures
			if (t_id.notNull() && t_id != gTextureBlank &&
				t_id != gTextureInvisible)
 			{
				if (!mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}
			}

			// Materials
			LLMaterial* matp = tep->getMaterialParams().get();
			if (matp)
			{
				has_materials = true;
				this_te_mat_llsd = matp->asLLSD();
				// Add the face number this material is used for.
				this_te_mat_llsd["face"] = i;

				t_id = validateTextureID(matp->getNormalID());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				t_id = validateTextureID(matp->getSpecularID());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				te_mat_llsd.append(this_te_mat_llsd);
			}

			// PBR materials (GLTF-encoded).
			LLGLTFMaterial* gltfp = tep->getGLTFMaterialOverride();
			if (gltfp)
			{
				has_pbr_mats = true;

				this_te_mat_llsd.clear();

				// Add the face number entry this material is used for.
				this_te_mat_llsd["face"] = i;

				const LLUUID& mat_id = objectp->getRenderMaterialID(i);
				if (mat_id.notNull())
				{
					// Add the material asset id.
					this_te_mat_llsd["mat_id"] = mat_id;
				}

				this_te_mat_llsd["json"] = gltfp->asJSON();

				t_id = validateTextureID(gltfp->getBaseColorId());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				t_id = validateTextureID(gltfp->getNormalId());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				t_id = validateTextureID(gltfp->getMetallicRoughnessId());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				t_id = validateTextureID(gltfp->getEmissiveId());
				if (t_id.notNull() && !mTexturesList.count(t_id))
				{
					mTexturesList.emplace(t_id);
				}

				te_gltf_llsd.append(this_te_mat_llsd);
			}
		}
		prim_llsd["textures"] = te_llsd;
		if (has_materials)
		{
			prim_llsd["materials"] = te_mat_llsd;
		}
		if (has_pbr_mats)
		{
			prim_llsd["gltf_materials"] = te_gltf_llsd;
		}

		// The keys in the primitive maps do not have to be localids, they can
		// be any string. We simply use localids because they are a unique
		// identifier
		snprintf(localid, sizeof(localid), "%u", objectp->getLocalID());
		llsd[(const char*)localid] = prim_llsd;
	}

	updateExportNumbers();

	return llsd;
}

void HBObjectBackup::exportNextTexture()
{
	LLUUID id;
	uuid_list_t::iterator iter = mTexturesList.begin();
	while (true)
	{
		if (mTexturesList.empty())
		{
			mCheckNextTexture = true;
			llinfos << "Finished exporting textures." << llendl;
			return;
		}
		if (iter == mTexturesList.end())
		{
			// Not yet ready, wait and re-check at next idle callback...
			mCheckNextTexture = true;
			return;
		}

		id = *iter++;
		if (id.isNull())
		{
			// NULL texture id: just remove and ignore.
			mTexturesList.erase(id);
			LL_DEBUGS("ObjectBackup") << "Null texture UUID found, ignoring."
									  << LL_ENDL;
			continue;
		}

		LLViewerTexture* imagep = LLViewerTextureManager::findTexture(id);
		if (!imagep)
		{
			llwarns << "We *DO NOT* have the texture " << id << llendl;
			mNonExportedTextures |= TEXTURE_MISSING;
			mTexturesList.erase(id);
			continue;
		}

		if (imagep->getDiscardLevel() <= 0)
		{
			// Texture is ready !
			break;
		}

		// Boost texture loading
		imagep->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
		LL_DEBUGS("ObjectBackup") << "Boosting texture: " << id << LL_ENDL;
		LLViewerFetchedTexture* texp =
			LLViewerTextureManager::staticCast(imagep);
		if (texp && texp->getDesiredDiscardLevel() > 0)
		{
			// Set min discard level to 0
			texp->setMinDiscardLevel(0);
			LL_DEBUGS("ObjectBackup") << "Min discard level set to 0 for texture: "
									  << id << LL_ENDL;
		}
	}

	mTexturesList.erase(id);

	llinfos << "Requesting texture " << id << " from cache." << llendl;
	LLImageJ2C* mFormattedImage = new LLImageJ2C;
	BackupCacheReadResponder* responder;
	responder = new BackupCacheReadResponder(id, mFormattedImage);
  	gTextureCachep->readFromCache(id, 0, 999999, responder);
}

//static
void HBObjectBackup::importCallback(HBFileSelector::ELoadFilter type,
									std::string& filename, void* datap)
{
	if (!filename.empty())
	{
		HBObjectBackup* self = getInstance();
		if (self)
		{
			self->mRetexture = (bool)(intptr_t)datap;
			self->doImportObject(filename);
		}
	}
}

//static
void HBObjectBackup::importObject(bool upload)
{
	if (findInstance())
	{
		llwarns << "Backup operation already in progress !" << llendl;
		showInstance();
	}
	else
	{
		HBFileSelector::loadFile(HBFileSelector::FFLOAD_XML, importCallback,
								 (void*)upload);
	}
}

bool HBObjectBackup::uploadNeeded(const LLUUID& id)
{
	// Is this asset a default texture ?
	if (id.isNull() || id == gTexturePlywood || id == gTextureBlank ||
		id == gTextureInvisible)
	{
		return false;
	}
	// Did we already register it for upload ?
	if (mTexturesList.count(id))
	{
		return false;
	}
	// Did we already upload and remap it during this session ?
	if (sAssetMap.count(id))
	{
		return false;
	}
	// Do we already have a usable inventory item for this asset ?
	return validateAssetPerms(id, true);
}

void HBObjectBackup::doImportObject(std::string filename)
{
	mTexturesList.clear();
	mCurrentAsset.setNull();

	mGotExtraPhysics = gAgent.hasRegionCapability("GetObjectPhysicsData");

	setDefaultTextures();

	mFolder = gDirUtilp->getDirName(filename) + LL_DIR_DELIM_STR;
	llifstream import_file(filename.c_str());
	bool success = import_file.is_open();
	if (success)
	{
		LLSDSerialize::fromXML(mLLSD, import_file);
		import_file.close();
		success = mLLSD.has("data");
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for reading."
				<< llendl;
	}
	if (!success)
	{
		gNotifications.add("ImportFailed");
		destroy();
		return;
	}

	showFloater(false);

	mAgentPos = gAgent.getPositionAgent();
	mAgentRot = LLQuaternion(gAgent.getAtAxis(), gAgent.getLeftAxis(),
							 gAgent.getUpAxis());

	// Get the texture map

	mCurObject = 1;
	mCurPrim = 1;
	mObjects = mLLSD["data"].size();
	mPrims = 0;
	mRezCount = 0;
	updateImportNumbers();

	if (!mRetexture)
	{
		importFirstObject();
		return;
	}

	std::string errmsg, warnmsg;
	for (LLSD::array_const_iterator prim_arr_it = mLLSD["data"].beginArray(),
									prim_arr_end = mLLSD["data"].endArray();
		 prim_arr_it != prim_arr_end; ++prim_arr_it)
	{
		LLSD llsd2 = (*prim_arr_it)["group_body"];

		for (LLSD::map_const_iterator prim_it = llsd2.beginMap(),
									  prim_end = llsd2.endMap();
			 prim_it != prim_end; ++prim_it)
		{
			LLSD prim_llsd = llsd2[prim_it->first];
			if (prim_llsd.has("sculpt"))
			{
				LLSculptParams sculpt;
				sculpt.fromLLSD(prim_llsd["sculpt"]);
				if ((sculpt.getSculptType() & LL_SCULPT_TYPE_MASK) !=
						LL_SCULPT_TYPE_MESH)
				{
					const LLUUID& s_id = sculpt.getSculptTexture();
					if (uploadNeeded(s_id))
					{
						llinfos << "Found a new sculpt texture to upload "
								<< s_id << llendl;
						mTexturesList.emplace(s_id);
					}
				}
			}

			if (prim_llsd.has("light_texture"))
			{
				LLLightImageParams lightimg;
				lightimg.fromLLSD(prim_llsd["light_texture"]);
				const LLUUID& l_id = lightimg.getLightTexture();
				if (uploadNeeded(l_id))
				{
					llinfos << "Found a new light texture to upload: " << l_id
							<< llendl;
					mTexturesList.emplace(l_id);
				}
			}

			// Check both for "textures" and "texture" since the second (buggy)
			// case has already been seen in some exported prims XML files...
			LLSD te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
													 : prim_llsd["texture"];
			for (LLSD::array_iterator it = te_llsd.beginArray();
				 it != te_llsd.endArray(); ++it)
			{
				LLSD the_te = *it;
				LLTextureEntry te;
				te.fromLLSD(the_te);

				LLUUID t_id = te.getID();
				if (uploadNeeded(t_id))
				{
					llinfos << "Found a new texture to upload: " << t_id
							<< llendl;
					mTexturesList.emplace(t_id);
				}
			}

			if (prim_llsd.has("materials"))
			{
				LLSD mat_llsd = prim_llsd["materials"];
				for (LLSD::array_iterator it = mat_llsd.beginArray();
					 it != mat_llsd.endArray(); ++it)
				{
					LLSD the_mat = *it;
					LLMaterial mat;
					mat.fromLLSD(the_mat);

					const LLUUID& n_id = mat.getNormalID();
					if (uploadNeeded(n_id))
 					{
						llinfos << "Found a new normal map to upload: "
								<< n_id << llendl;
						mTexturesList.emplace(n_id);
					}

					const LLUUID& s_id = mat.getSpecularID();
					if (uploadNeeded(s_id))
 					{
						llinfos << "Found a new specular map to upload: "
								<< s_id << llendl;
						mTexturesList.emplace(s_id);
					}
				}
			}

			if (!prim_llsd.has("gltf_materials"))
			{
				continue;
			}
			LLSD mat_llsd = prim_llsd["gltf_materials"];
			if (mat_llsd.has("mat_id"))
			{
				LLUUID mat_id = mat_llsd["mat_id"].asUUID();
				if (mat_id.notNull() &&
					(sAssetMap.count(mat_id) ||
					 validateAssetPerms(mat_id, true)))
				{
					// We have the corresponding PBR material asset in our
					// inventory, so we do not need to upload the associated
					// textures.
					continue;
				}
			}
			for (LLSD::array_iterator it = mat_llsd.beginArray();
				 it != mat_llsd.endArray(); ++it)
			{
				LLSD the_mat = *it;
				LLGLTFMaterial mat;
				if (!mat.fromJSON(the_mat["json"], warnmsg, errmsg))
				{
					llwarns << "Failed GLTF from JSON decoding: "
							<< (errmsg.empty() ? warnmsg : errmsg)
							<< llendl;
					continue;
				}

				const LLUUID& b_id = mat.getBaseColorId();
				if (uploadNeeded(b_id))
				{
					llinfos << "Found a new base color map to upload: " << b_id
							<< llendl;
					mTexturesList.emplace(b_id);
				}

				const LLUUID& n_id = mat.getNormalId();
				if (uploadNeeded(n_id))
				{
					llinfos << "Found a new normal map to upload: " << n_id
							<< llendl;
					mTexturesList.emplace(n_id);
				}

				const LLUUID& m_id = mat.getMetallicRoughnessId();
				if (uploadNeeded(m_id))
				{
					llinfos << "Found a new metallic roughness map to upload: "
							<< m_id << llendl;
					mTexturesList.emplace(m_id);
				}

				const LLUUID& e_id = mat.getEmissiveId();
				if (uploadNeeded(e_id))
				{
					llinfos << "Found a new emissive map to upload: " << e_id
							<< llendl;
					mTexturesList.emplace(e_id);
				}
			}
		}
	}

	uploadNextAsset();
}

LLVector3 HBObjectBackup::offsetAgent(LLVector3 offset)
{
	return offset * mAgentRot + mAgentPos;
}

void HBObjectBackup::rezAgentOffset(LLVector3 offset)
{
	// This will break for a sitting agent
	LLToolPlacer mPlacer;
	mPlacer.setObjectType(LL_PCODE_CUBE);
	mPlacer.placeObject((S32)offset.mV[0], (S32)offset.mV[1], MASK_NONE);
}

void HBObjectBackup::importFirstObject()
{
	mRunning = true;
	showFloater(false);
	mGroupPrimImportIter = mLLSD["data"].beginArray();
	mRootRootPos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mObjects = mLLSD["data"].size();
	mCurObject = 1;
	importNextObject();
}

void HBObjectBackup::importNextObject()
{
	mToSelect.clear();
	mRezCount = 0;

	mThisGroup = (*mGroupPrimImportIter)["group_body"];
	mPrimImportIter = mThisGroup.beginMap();

	mCurPrim = 0;
	mPrims = mThisGroup.size();
	updateImportNumbers();

	LLVector3 lgpos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mGroupOffset = lgpos - mRootRootPos;
	mRootPos = offsetAgent(LLVector3(2.f, 0.f, 0.f));
	mRootRot = ll_quaternion_from_sd((*mGroupPrimImportIter)["root_rotation"]);

	rezAgentOffset(LLVector3(0.f, 2.f, 0.f));
	// Now we must wait for the callback when ViewerObjectList gets the new
	// objects and we have the correct number selected
}

class HBBackupMatInvCB final : public LLInventoryCallback
{
protected:
	LOG_CLASS(HBBackupMatInvCB);

public:
	HBBackupMatInvCB(const LLUUID& object_id, const LLUUID& mat_id, S32 face,
					 const std::string& name, const std::string& buffer)
	:	mFace(face),
		mObjectId(object_id),
		mOriginalMatId(mat_id),
		mItemName(name),
		mBuffer(buffer)
	{
	}

	void fire(const LLUUID& inv_item_id) override
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(inv_item_id);
		if (!itemp) return;

		// Hold a pointer on self to avoid getting destroyed on fire() exit.
		mSelf = this;

		// create_inventory_item() does not allow presetting some permissions;
		// fix it now.
		LLPermissions perms;
		perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		itemp->setPermissions(perms);
		itemp->updateServer(false);
		gInventory.updateItem(itemp);
		gInventory.notifyObservers();

		if (itemp->getName() != mItemName)
		{
			LLSD updates;
			updates["name"] = mItemName;
			update_inventory_item(inv_item_id, updates, NULL);
		}

		LLResourceUploadInfo::ptr_t infop =
		std::make_shared<LLBufferedAssetUploadInfo>(
			inv_item_id, LLAssetType::AT_MATERIAL, mBuffer,
			boost::bind(&HBBackupMatInvCB::uploadDone, _2, this),
			boost::bind(&HBBackupMatInvCB::uploadFailed, this));
		const std::string& cap_url =
			gAgent.getRegionCapability("UpdateMaterialAgentInventory");
		LLViewerAssetUpload::enqueueInventoryUpload(cap_url, infop);
	}

	// Applies the material asset to the face, once it has been created.
	static void uploadDone(LLUUID asset_id, HBBackupMatInvCB* self)
	{
		// Remember the mapped Id for the original material we recreated.
		HBObjectBackup::sAssetMap.emplace(self->mOriginalMatId, asset_id);
		// Set the recreated material to the object face.
		LLViewerObject* objectp = gObjectList.findObject(self->mObjectId);
		if (objectp && !objectp->isDead())
		{
			objectp->setRenderMaterialID(self->mFace, asset_id);
		}
		self->mSelf = NULL;	// Commit suicide.
	}

	static void uploadFailed(HBBackupMatInvCB* self)
	{
		self->mSelf = NULL;	// Commit suicide.
	}

private:
	S32							mFace;
	LLPointer<HBBackupMatInvCB>	mSelf;
	LLUUID						mObjectId;
	LLUUID						mOriginalMatId;
	std::string					mItemName;
	std::string					mBuffer;
};

static void create_inventory_mat_item(const LLUUID& obj_id,
									  const LLUUID& mat_id,
									  S32 te, LLGLTFMaterial& mat)
{
	if (!gAgent.hasInventoryMaterial())
	{
		return;
	}

	std::string name;
	if (mat_id.notNull())
	{
		name = "Material " + mat_id.asString();
	}
	else
	{
		name = " Object " + obj_id.asString() + " material";
	}

	LLSD asset;
	asset["version"] = LLGLTFMaterial::ASSET_VERSION;
	asset["type"] = LLGLTFMaterial::ASSET_TYPE;
	asset["data"] = mat.asJSON();

	std::stringstream buffer;
	LLSDSerialize::serialize(asset, buffer, LLSDSerialize::LLSD_BINARY);

	LLTransactionID tid;
	tid.generate();

	LLUUID parent_id =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_MATERIAL);

	LLPermissions perms;
	perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);

	LLPointer<LLInventoryCallback> cb = new HBBackupMatInvCB(obj_id, mat_id,
															 te, name,
															 buffer.str());

	create_inventory_item(parent_id, tid, name, name,
						  LLAssetType::AT_MATERIAL,
						  LLInventoryType::IT_MATERIAL, NO_INV_SUBTYPE,
						  perms.getMaskNextOwner(), cb);
}

// This function takes a pointer to a viewer object and applies the prim
// definition that prim_llsd has
void HBObjectBackup::xmlToPrim(LLSD prim_llsd, LLViewerObject* object)
{
	const LLUUID& id = object->getID();
	mExpectingUpdate = id;
	gSelectMgr.selectObjectAndFamily(object);

	if (prim_llsd.has("name"))
	{
		gSelectMgr.selectionSetObjectName(prim_llsd["name"]);
	}

	if (prim_llsd.has("description"))
	{
		gSelectMgr.selectionSetObjectDescription(prim_llsd["description"]);
	}

	if (prim_llsd.has("material"))
	{
		gSelectMgr.selectionSetMaterial(prim_llsd["material"].asInteger());
	}

	if (prim_llsd.has("clickaction"))
	{
		gSelectMgr.selectionSetClickAction(prim_llsd["clickaction"].asInteger());
	}

	if (prim_llsd.has("parent"))
	{
		// We are not the root node.
		LLVector3 pos = LLVector3(prim_llsd["position"]);
		LLQuaternion rot = ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setPositionRegion(pos * mRootRot + mRootPos + mGroupOffset);
		object->setRotation(rot * mRootRot);
	}
	else
	{
		object->setPositionRegion(mRootPos + mGroupOffset);
		LLQuaternion rot=ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setRotation(rot);
	}

	object->setScale(LLVector3(prim_llsd["scale"]));

	if (prim_llsd.has("flags"))
	{
		U32 flags = (U32)prim_llsd["flags"].asInteger();
		object->setFlags(flags, true);
	}
	else	// Kept for backward compatibility
	{
		if (prim_llsd.has("phantom") && prim_llsd["phantom"].asInteger() == 1)
		{
			object->setFlags(FLAGS_PHANTOM, true);
		}

		if (prim_llsd.has("physical") &&
			prim_llsd["physical"].asInteger() == 1)
		{
			object->setFlags(FLAGS_USE_PHYSICS, true);
		}
	}

	if (mGotExtraPhysics && prim_llsd.has("ExtraPhysics"))
	{
		const LLSD& physics = prim_llsd["ExtraPhysics"];
		object->setPhysicsShapeType(physics["PhysicsShapeType"].asInteger());
		F32 gravity = physics.has("Gravity") ? physics["Gravity"].asReal()
											 : physics["GravityMultiplier"].asReal();
		object->setPhysicsGravity(gravity);
		object->setPhysicsFriction(physics["Friction"].asReal());
		object->setPhysicsDensity(physics["Density"].asReal());
		object->setPhysicsRestitution(physics["Restitution"].asReal());
		object->updateFlags(true);
	}

	// Volume params
	LLVolumeParams volume_params = object->getVolume()->getParams();
	volume_params.fromLLSD(prim_llsd["volume"]);
	object->updateVolume(volume_params);

	if (prim_llsd.has("sculpt"))
	{
		LLSculptParams sculpt;
		sculpt.fromLLSD(prim_llsd["sculpt"]);

		// *TODO: check if map is valid and only set texture if map is valid
		// and changes
		const LLUUID& t_id = sculpt.getSculptTexture();
		rebase_map_t::const_iterator it = sAssetMap.find(t_id);
		if (it != sAssetMap.end())
		{
			sculpt.setSculptTexture(it->second, LL_SCULPT_TYPE_MESH);
		}

		object->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt, true);
	}

	if (prim_llsd.has("light"))
	{
		LLLightParams light;
		light.fromLLSD(prim_llsd["light"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT, light, true);
	}
	if (prim_llsd.has("light_texture"))
	{
		// Light image
		LLLightImageParams lightimg;
		lightimg.fromLLSD(prim_llsd["light_texture"]);
		const LLUUID& t_id = lightimg.getLightTexture();
		rebase_map_t::const_iterator it = sAssetMap.find(t_id);
		if (it != sAssetMap.end())
		{
			lightimg.setLightTexture(it->second);
		}
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT_IMAGE, lightimg,
								  true);
	}

	if (prim_llsd.has("flexible"))
	{
		LLFlexibleObjectData flex;
		flex.fromLLSD(prim_llsd["flexible"]);
		object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, flex, true);
	}

	// Textures
	// Check both for "textures" and "texture" since the second (buggy) case
	// has already been seen in some exported prims XML files...
	llinfos << "Processing textures for prim " << id << llendl;
	LLSD te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
											 : prim_llsd["texture"];
	U8 i = 0;
	for (LLSD::array_iterator it = te_llsd.beginArray();
		 it != te_llsd.endArray(); ++it)
	{
	    LLSD the_te = *it;
	    LLTextureEntry te;
	    te.fromLLSD(the_te);
		const LLUUID& t_id = te.getID();
		rebase_map_t::const_iterator tit = sAssetMap.find(t_id);
		if (tit != sAssetMap.end())
		{
			te.setID(tit->second);
		}

	    object->setTE(i++, te);
	}
	llinfos << "Textures done !" << llendl;

	// Legacy materials
	if (prim_llsd.has("materials"))
	{
		llinfos << "Processing legacy materials for prim " << id << llendl;
		te_llsd = prim_llsd["materials"];
		// Note: old export format lacked a texture entry reference and
		// therefore failed to properly export objects with mixed materials and
		// non-materials faces. For these, we just increment the face number
		// (i) for each new material found in the exported data, hoping there
		// will be no "hole"...
		bool missing_te_ref = false;
		i = 0;
		for (LLSD::array_iterator it = te_llsd.beginArray();
			 it != te_llsd.endArray(); ++it)
		{
		    LLSD the_mat = *it;
			if (the_mat.has("face"))
			{
				S32 te = the_mat["face"].asInteger();
				if (te >= 0 && te < 256)	// Paranoia
				{
					i = te;
				}
				else
				{
					llwarns << "Bad face number (" << te
							<< "): material skipped." << llendl;
					continue;
				}
			}
			else
			{
				missing_te_ref = true;
			}
			LLMaterialPtr matp = new LLMaterial(the_mat);

			const LLUUID& n_id = matp->getNormalID();
			if (n_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(n_id);
				if (tit != sAssetMap.end())
				{
					matp->setNormalID(tit->second);
				}
			}

			const LLUUID& s_id = matp->getSpecularID();
			if (s_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(n_id);
				if (tit != sAssetMap.end())
				{
					matp->setSpecularID(tit->second);
				}
			}

			LLMaterialMgr::getInstance()->put(id, i++, *matp);
		}
		if (missing_te_ref)
		{
			llwarns << "Legacy materials done, but the exported file got missing face number references for them: they have been set in sequence, which only works for objects not mixing materials and non-materials faces."
					<< llendl;
		}
		else
		{
			llinfos << "Legacy materials done !" << llendl;
		}
	}

	// PBR (GLTF-encoded) materials
	if (prim_llsd.has("gltf_materials"))
	{
		std::string warnmsg, errmsg;
		llinfos << "Processing PBR materials for prim " << id << llendl;
		te_llsd = prim_llsd["gltf_materials"];
		for (LLSD::array_iterator it = te_llsd.beginArray();
			 it != te_llsd.endArray(); ++it)
		{
		    LLSD the_mat = *it;
			if (!the_mat.has("face") || !the_mat.has("json"))
			{
				llwarns << "Malformed gltf_materials LLSD entry. Skipping."
						<< llendl;
				continue;
			}
			U8 i = the_mat["face"].asInteger();

			LLUUID mat_id;
			if (te_llsd.has("mat_id"))
			{
				mat_id = te_llsd["mat_id"].asUUID();
				if (mat_id.notNull())
				{
					// Check to see if we already created a new material for
					// this saved material Id.
					rebase_map_t::const_iterator tit = sAssetMap.find(mat_id);
					if (tit != sAssetMap.end())
					{
						mat_id = tit->second;
					}
					// Check to see if we have the original material in our
					// inventory. If not, reset Id to null.
					else if (!validateAssetPerms(mat_id, true))
					{
						mat_id.setNull();
					}
				}
				if (mat_id.notNull())
				{
					// We have the corresponding PBR material asset in our
					// inventory already, so simply apply it.
					object->setRenderMaterialID(i, mat_id);
					continue;
				}
			}

			LLGLTFMaterial mat;
			if (!mat.fromJSON(the_mat["json"], warnmsg, errmsg))
			{
				llwarns << "Failed GLTF from JSON decoding: "
						<< (errmsg.empty() ? warnmsg : errmsg) << llendl;
				continue;
			}

			const LLUUID& b_id = mat.getBaseColorId();
			if (b_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(b_id);
				if (tit != sAssetMap.end())
				{
					mat.setBaseColorId(tit->second);
				}
			}

			const LLUUID& n_id = mat.getNormalId();
			if (n_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(n_id);
				if (tit != sAssetMap.end())
				{
					mat.setNormalId(tit->second);
				}
			}

			const LLUUID& m_id = mat.getMetallicRoughnessId();
			if (m_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(m_id);
				if (tit != sAssetMap.end())
				{
					mat.setMetallicRoughnessId(tit->second);
				}
			}

			const LLUUID& e_id = mat.getEmissiveId();
			if (e_id.notNull())
			{
				rebase_map_t::const_iterator tit = sAssetMap.find(e_id);
				if (tit != sAssetMap.end())
				{
					mat.setEmissiveId(tit->second);
				}
			}

			create_inventory_mat_item(id, mat_id, i, mat);
		}
		llinfos << "PBR materials done !" << llendl;
	}

	object->sendTEUpdate();
	object->sendShapeUpdate();

	// There is a server bug preventing to update the scale, position and
	// rotation at once...
	static LLCachedControl<bool> multiple_update_bug(gSavedSettings,
													 "MultipleUpdateBug");
	if (multiple_update_bug)
	{
		gSelectMgr.sendMultipleUpdate(UPD_SCALE);
		gSelectMgr.sendMultipleUpdate(UPD_POSITION | UPD_ROTATION);
	}
	else
	{
		gSelectMgr.sendMultipleUpdate(UPD_SCALE | UPD_POSITION | UPD_ROTATION);
	}

	gSelectMgr.deselectAll();
}

// This is fired when the update packet is processed so we know the prim
// settings have stuck
//static
void HBObjectBackup::primUpdate(LLViewerObject* object)
{
	HBObjectBackup* self = findInstance();
	if (!object || object->isDead() || !self || !self->mRunning ||
		object->mID != self->mExpectingUpdate)
	{
		return;
	}

	++self->mCurPrim;
	self->updateImportNumbers();
	++self->mPrimImportIter;

	self->mExpectingUpdate.setNull();

	if (self->mPrimImportIter == self->mThisGroup.endMap())
	{
		llinfos << "Trying to link..." << llendl;

		if (self->mToSelect.size() > 1)
		{
			std::reverse(self->mToSelect.begin(), self->mToSelect.end());
			// Now link
			gSelectMgr.deselectAll();
			gSelectMgr.selectObjectAndFamily(self->mToSelect, true);
			gSelectMgr.sendLink();
			LLViewerObject* root = self->mToSelect.back();
			root->setRotation(self->mRootRot);
		}

		++self->mCurObject;
		++self->mGroupPrimImportIter;
		if (self->mGroupPrimImportIter != self->mLLSD["data"].endArray())
		{
			self->importNextObject();
			return;
		}

		self->mRunning = false;
		self->destroy();
		return;
	}

	LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];

	if (self->mToSelect.empty())
	{
		llwarns << "error: ran out of objects to mod." << llendl;
		self->mRunning = false;
		self->destroy();
		return;
	}

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		//rezAgentOffset(LLVector3(1.f, 0.f, 0.f));
		LLSD prim_llsd =self-> mThisGroup[self->mPrimImportIter->first];
		++self->mProcessIter;
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

// Callback when we rez a new object when the importer is running.
//static
void HBObjectBackup::newPrim(LLViewerObject* object)
{
	HBObjectBackup* self = findInstance();
	if (!object || object->isDead() || !self || !self->mRunning) return;

	++self->mRezCount;
	self->mToSelect.push_back(object);
	self->updateImportNumbers();
	++self->mPrimImportIter;

	object->setPositionLocal(self->offsetAgent(LLVector3(0.f, 1.f, 0.f)));
	gSelectMgr.sendMultipleUpdate(UPD_POSITION);

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		self->rezAgentOffset(LLVector3(1.f, 0.f ,0.f));
	}
	else
	{
		llinfos << "All prims rezzed, moving to build stage" << llendl;
		// Deselecting is required to ensure that the first child prim in
		// the link set (which is also the last rezzed prim and thus
		// currently selected) will be properly renamed and desced.
		gSelectMgr.deselectAll();
		self->mPrimImportIter = self->mThisGroup.beginMap();
		LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];
		self->mProcessIter = self->mToSelect.begin();
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

void HBObjectBackup::updateMap(const LLUUID& uploaded_asset)
{
	if (mCurrentAsset.notNull())
	{
		llinfos << "Mapping " << mCurrentAsset << " to " << uploaded_asset
				<< llendl;
		sAssetMap.emplace(mCurrentAsset, uploaded_asset);
	}
}

void HBObjectBackup::uploadNextAsset()
{
	if (gAgent.getRegionCapability("NewFileAgentInventory").empty() &&
		!mTexturesList.empty())
	{
		llwarns << "NewAgentInventory capability not found. Cannot upload !"
				<< llendl;
		mTexturesList.clear();
	}

	if (mTexturesList.empty())
	{
		llinfos << "Texture list is empty, moving to rez stage." << llendl;
		mCurrentAsset.setNull();
		importFirstObject();
		return;
	}

	updateImportNumbers();

	uuid_list_t::iterator iter = mTexturesList.begin();
	LLUUID id = *iter;
	mTexturesList.erase(iter);

	llinfos << "Got texture ID " << id << ": trying to upload..." << llendl;

	mCurrentAsset = id;
	std::string struid;
	id.toString(struid);
	std::string filename = mFolder + struid;
	LLAssetID uuid;
	LLTransactionID tid;

	// Generate a new transaction ID for this asset
	tid.generate();
	uuid = tid.makeAssetID(gAgent.getSecureSessionID());

	S64 file_size;
	LLFile infile(filename, "rb", &file_size);
	if (!infile)
	{
		llwarns << "Unable to access input file " << filename << llendl;
		uploadNextAsset();
		return;
	}

	constexpr S32 buf_size = 65536;
	U8 copy_buf[buf_size];
	LLFileSystem file(uuid, LLFileSystem::APPEND);
	while ((file_size = infile.read(copy_buf, buf_size)))
	{
		file.write(copy_buf, file_size);
	}

	S32 upload_cost = LLEconomy::getInstance()->getTextureUploadCost();

	LLResourceUploadInfo::ptr_t
		info(new LLResourceUploadInfo(tid, LLAssetType::AT_TEXTURE,
									  struid, struid, 0,
									  LLFolderType::FT_TEXTURE,
									  LLInventoryType::IT_TEXTURE,
									  LLFloaterPerms::getNextOwnerPerms(),
									  LLFloaterPerms::getGroupPerms(),
									  LLFloaterPerms::getEveryonePerms(),
									  upload_cost));
	info->setCapCallback(uploadNextAssetCallback, NULL);
	info->setShowInventoryPanel(false);
	upload_new_resource(info);
}

// Recursively calls uploadNextAsset()... *TODO: turn the whole import process
// into an idle callback worker, like for the export one...
//static
void HBObjectBackup::uploadNextAssetCallback(const LLSD& result, void*)
{
	HBObjectBackup* self = HBObjectBackup::findInstance();
	if (self)
	{
		self->updateMap(result["new_asset"].asUUID());
		self->uploadNextAsset();
	}
	else
	{
		llwarns << "Import aborted, HBObjectBackup instance gone !" << llendl;
	}
}
