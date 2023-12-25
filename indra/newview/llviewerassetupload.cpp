/**
 * @file llviewerassetupload.cpp
 * @brief Asset upload requests.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llviewerassetupload.h"

#include "llcoproceduremanager.h"
#include "lldir.h"
#include "lldatapacker.h"
#include "lleconomy.h"
#include "llfilesystem.h"
#include "llkeyframemotion.h"
#include "llnotifications.h"
#include "lltransactiontypes.h"			// For TRANS_UPLOAD_CHARGE
#include "llsdutil.h"
#include "lltrans.h"
#include "lluploaddialog.h"
#include "llvorbisencode.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"				// For gDisconnected
#include "llfloaterbuycurrency.h"
#include "llfloaterinventory.h"
#include "llfloaterperms.h"
#include "llfolderview.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llinventoryactions.h"			// For open_texture()
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llselectmgr.h"				// For dialog_refresh_all()
#include "llstatusbar.h"				// For money balance
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llvoavatarself.h"

constexpr S32 MAX_PREVIEWS = 5;

// Multiple uploads
std::deque<std::string> gUploadQueue;
LLMutex gUploadQueueMutex;

// Helper function
S32 upload_cost_for_asset_type(LLAssetType::EType type)
{
	if (type == LLAssetType::AT_TEXTURE)
	{
		return LLEconomy::getInstance()->getTextureUploadCost();
	}
	if (type == LLAssetType::AT_SOUND)
	{
		return LLEconomy::getInstance()->getSoundUploadCost();
	}
	if (type == LLAssetType::AT_ANIMATION)
	{
		return LLEconomy::getInstance()->getAnimationUploadCost();
	}
	if (type == LLAssetType::AT_MESH || type == LLAssetType::AT_NONE)
	{
		return LLEconomy::getInstance()->getPriceUpload();
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// LLResourceUploadInfo class
///////////////////////////////////////////////////////////////////////////////

constexpr U32 LL_ASSET_UPLOAD_TIMEOUT_SEC = 60;

LLResourceUploadInfo::LLResourceUploadInfo(const LLTransactionID& tid,
										   LLAssetType::EType asset_type,
										   const std::string& name,
										   const std::string& description,
										   S32 compression_info,
										   LLFolderType::EType dest_type,
										   LLInventoryType::EType inv_type,
										   U32 perms_next, U32 perms_group,
										   U32 perms_everyone, S32 cost)
:	mTransactionId(tid),
	mAssetType(asset_type),
	mName(name),
	mDescription(description),
	mCompressionInfo(compression_info),
	mDestinationFolderType(dest_type),
	mInventoryType(inv_type),
	mNextOwnerPerms(perms_next),
	mGroupPerms(perms_group),
	mEveryonePerms(perms_everyone),
	mExpectedUploadCost(cost),
	mCapCallback(NULL),
	mUserData(NULL),
	mShowInventoryPanel(true)
{
}

LLResourceUploadInfo::LLResourceUploadInfo(const std::string& name,
										   const std::string& description,
										   S32 compression_info,
										   LLFolderType::EType dest_type,
										   LLInventoryType::EType inv_type,
										   U32 perms_next, U32 perms_group,
										   U32 perms_everyone, S32 cost)
:	mName(name),
	mDescription(description),
	mCompressionInfo(compression_info),
	mDestinationFolderType(dest_type),
	mInventoryType(inv_type),
	mNextOwnerPerms(perms_next),
	mGroupPerms(perms_group),
	mEveryonePerms(perms_everyone),
	mExpectedUploadCost(cost),
	mAssetType(LLAssetType::AT_NONE),
	mCapCallback(NULL),
	mUserData(NULL),
	mShowInventoryPanel(true)
{
	mTransactionId.generate();
}

LLResourceUploadInfo::LLResourceUploadInfo(const LLAssetID& asset_id,
										   LLAssetType::EType asset_type,
										   const std::string& name)
:	mAssetId(asset_id),
	mAssetType(asset_type),
	mName(name),
	mCompressionInfo(0),
	mDestinationFolderType(LLFolderType::FT_NONE),
	mInventoryType(LLInventoryType::IT_NONE),
	mNextOwnerPerms(0),
	mGroupPerms(0),
	mEveryonePerms(0),
	mExpectedUploadCost(0),
	mCapCallback(NULL),
	mUserData(NULL),
	mShowInventoryPanel(true)
{
}

//virtual
LLSD LLResourceUploadInfo::prepareUpload()
{
	if (mAssetId.isNull())
	{
		generateNewAssetId();
	}

	incrementUploadStats();
	assignDefaults();

	return LLSD().with("success", LLSD::Boolean(true));
}

std::string LLResourceUploadInfo::getAssetTypeString() const
{
	return LLAssetType::lookup(mAssetType);
}

std::string LLResourceUploadInfo::getInventoryTypeString() const
{
	return LLInventoryType::lookup(mInventoryType);
}

//virtual
LLSD LLResourceUploadInfo::generatePostBody()
{
	LLSD body;

	body["folder_id"] = mFolderId;
	body["asset_type"] = getAssetTypeString();
	body["inventory_type"] = getInventoryTypeString();
	body["name"] = mName;
	body["description"] = mDescription;
	body["next_owner_mask"] = LLSD::Integer(mNextOwnerPerms);
	body["group_mask"] = LLSD::Integer(mGroupPerms);
	body["everyone_mask"] = LLSD::Integer(mEveryonePerms);

	return body;
}

//virtual
void LLResourceUploadInfo::logPreparedUpload()
{
	llinfos << "Uploading asset name: " << mName << " - Asset type: "
			<< LLAssetType::lookup(mAssetType) << " - Asset Id: " << mAssetId
			<< " - Description: " << mDescription
			<< " - Expected upload Cost: " << mExpectedUploadCost
			<< " - Folder Id: " << mFolderId << llendl;
}

//virtual
S32 LLResourceUploadInfo::getExpectedUploadCost()
{
	if (mExpectedUploadCost < 0)	// Unknown cost
	{
		mExpectedUploadCost = upload_cost_for_asset_type(mAssetType);
	}
	return mExpectedUploadCost;
}

//virtual
LLUUID LLResourceUploadInfo::finishUpload(const LLSD& result)
{
	if (getFolderId().isNull())
	{
		return LLUUID::null;
	}

	U32 perms_everyone = PERM_NONE;
	U32 perms_group = PERM_NONE;
	U32 perms_next = PERM_ALL;

	if (result.has("new_next_owner_mask"))
	{
		// The server provided creation permissions so use them. Do not assume
		// we got the permissions we asked for in since the server may not have
		// granted them all.
		perms_everyone = result["new_everyone_mask"].asInteger();
		perms_group = result["new_group_mask"].asInteger();
		perms_next = result["new_next_owner_mask"].asInteger();
	}
	// The server does not provide creation permissions so use old assumption-
	// based permissions.
	else if (getAssetTypeString() != "snapshot")
	{
		perms_next = PERM_MOVE | PERM_TRANSFER;
	}

	LLPermissions new_perms;
	new_perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
	new_perms.initMasks(PERM_ALL, PERM_ALL, perms_everyone, perms_group,
						perms_next);

	U32 inv_item_flags = 0;
	if (result.has("inventory_flags"))
	{
		inv_item_flags = (U32)result["inventory_flags"].asInteger();
		if (inv_item_flags)
		{
			llinfos << "Inventory item flags: " << inv_item_flags << llendl;
		}
	}

	S32 creation_date_now = time_corrected();
	LLUUID new_inv_item_id = result["new_inventory_item"].asUUID();

	LLPointer<LLViewerInventoryItem> item =
		new LLViewerInventoryItem(new_inv_item_id, getFolderId(), new_perms,
								  result["new_asset"].asUUID(), getAssetType(),
								  getInventoryType(), getName(),
								  getDescription(), LLSaleInfo::DEFAULT,
								  inv_item_flags, creation_date_now);

	gInventory.updateItem(item);
	gInventory.notifyObservers();

	return new_inv_item_id;
}

LLAssetID LLResourceUploadInfo::generateNewAssetId()
{
	if (gDisconnected)
	{
		LLAssetID rv;
		rv.setNull();
		return rv;
	}

	mAssetId = mTransactionId.makeAssetID(gAgent.getSecureSessionID());
	return mAssetId;
}

void LLResourceUploadInfo::incrementUploadStats() const
{
	if (mAssetType == LLAssetType::AT_SOUND)
	{
		gViewerStats.incStat(LLViewerStats::ST_UPLOAD_SOUND_COUNT);
	}
	else if (mAssetType == LLAssetType::AT_TEXTURE)
	{
		gViewerStats.incStat(LLViewerStats::ST_UPLOAD_TEXTURE_COUNT);
	}
	else if (mAssetType == LLAssetType::AT_ANIMATION)
	{
		gViewerStats.incStat(LLViewerStats::ST_UPLOAD_ANIM_COUNT);
	}
}

//virtual
void LLResourceUploadInfo::assignDefaults()
{
	if (mInventoryType == LLInventoryType::IT_NONE)
	{
		mInventoryType = LLInventoryType::defaultForAssetType(mAssetType);
	}
	LLStringUtil::stripNonprintable(mName);
	LLStringUtil::stripNonprintable(mDescription);
	if (mName.empty())
	{
		mName = "(No Name)";
	}
	if (mDescription.empty())
	{
		mDescription = "(No Description)";
	}

	LLFolderType::EType type;
	if (mDestinationFolderType == LLFolderType::FT_NONE)
	{
		type = (LLFolderType::EType)mAssetType;
	}
	else
	{
		type = mDestinationFolderType;
	}
	mFolderId = gInventory.findChoosenCategoryUUIDForType(type);
}

//virtual
std::string LLResourceUploadInfo::getDisplayName() const
{
	return mName.empty() ? mAssetId.asString() : mName;
}

void LLResourceUploadInfo::performCallback(const LLSD& result)
{
	if (mCapCallback)
	{
		mCapCallback(result, mUserData);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLNewFileResourceUploadInfo class
///////////////////////////////////////////////////////////////////////////////

LLNewFileResourceUploadInfo::LLNewFileResourceUploadInfo(const std::string& fname,
														 const std::string& name,
														 const std::string& desc,
														 S32 compression_info,
														 LLFolderType::EType dest_type,
														 LLInventoryType::EType inv_type,
														 U32 perms_next,
														 U32 perms_group,
														 U32 perms_everyone,
														 S32 cost)
:	LLResourceUploadInfo(name, desc, compression_info, dest_type, inv_type,
						 perms_next, perms_group, perms_everyone, cost),
	mFileName(fname)
{
}

//virtual
LLSD LLNewFileResourceUploadInfo::prepareUpload()
{
	if (getAssetId().isNull())
	{
		generateNewAssetId();
	}

	LLSD result = exportTempFile();
	if (result.has("error"))
	{
		return result;
	}

	return LLResourceUploadInfo::prepareUpload();
}

//virtual
LLSD LLNewFileResourceUploadInfo::exportTempFile()
{
	std::string filename = gDirUtilp->getTempFilename();

	const std::string& orig_filename = getFileName();
	std::string exten = gDirUtilp->getExtension(orig_filename);
	EImageCodec codec = LLImageBase::getCodecFromExtension(exten);

	LLAssetType::EType asset_type = LLAssetType::AT_NONE;

	LLSD args;
	std::string error_msg, error_label;
	bool error = false;

	if (exten.empty())
	{
		std::string short_name = gDirUtilp->getBaseFileName(filename);

		// No extension
		error_msg = "No file extension for the file: " + short_name + "\n";
		error_msg += "Please make sure the file has a correct file extension.\n";
		error_label = "NoFileExtension";
		args["FILE"] = short_name;
		error = true;
	}
	else if (codec != IMG_CODEC_INVALID)
	{
		// It is an image file, the upload procedure is the same for all
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(orig_filename, filename,
												   codec))
		{
			error_msg = "Problem with file '" + orig_filename + "':\n\n";
			error_msg += LLImage::getLastError() + "\n";
			error_label = "ProblemWithFile";
			args["FILE"] = orig_filename;
			args["ERROR"] = LLImage::getLastError();
			error = true;
		}
	}
	else if (exten == "wav" || exten == "dsf")
	{
		asset_type = LLAssetType::AT_SOUND;  // tag it as audio

		llinfos << "Attempting to encode wav as an ogg file" << llendl;

		F32 max_duration = 0.f; // 0 means using SL maximum duration default
		if (!gIsInSecondLife)
		{
			max_duration = gSavedSettings.getF32("OSMaxSoundDuration");
		}
		S32 encode_result = encode_vorbis_file(orig_filename, filename,
											   max_duration);
		if (encode_result != LLVORBISENC_NOERR)
		{
			switch (encode_result)
			{
				case LLVORBISENC_DEST_OPEN_ERR:
					error_msg = "Could create temporary Vorbis sound file: ";
					error_msg += filename + "\n";
					error_label = "CannotOpenTemporarySoundFile";
					args["FILE"] = filename;
					break;

				default:
					error_msg ="Unknown vorbis encode failure on: ";
					error_msg += orig_filename + "\n";
					error_label = "UnknownVorbisEncodeFailure";
					args["FILE"] = orig_filename;
			}
			error = true;
		}
	}
	else if (exten == "anim")
	{
		error_label = "GenericAlert";
		error = true;
		S64 size = 0;
		LLFile infile(orig_filename, "rb", &size);
		if (!infile)
		{
			error_msg = "Failed to open animation file: " + filename + "\n";
		}
		else if (size <= 0)
		{
			error_msg = "Animation file " + orig_filename + " is empty !\n";
		}
		else
		{
			U8* buffer = new U8[size];
			S64 size_read = infile.read(buffer, size);
			if (size_read != size)
			{
				error_msg =
					llformat("Failed to read animation file %s: wanted %d bytes, got %d\n",
							 orig_filename.c_str(), size, size_read);
			}
			else
			{
				LLDataPackerBinaryBuffer dp(buffer, size);
				LLKeyframeMotion* motionp = new LLKeyframeMotion(getAssetId());
				motionp->setCharacter(gAgentAvatarp);
				if (motionp->deserialize(dp, getAssetId(), false))
				{
					// Write to temporary file
					if (motionp->dumpToFile(filename))
					{
						asset_type = LLAssetType::AT_ANIMATION;
						error_label.clear();
						error = false;
					}
					else
					{
						error_msg = "Failed saving temporary animation file\n";
					}
				}
				else
				{
					error_msg = "Failed reading animation file: " +
								orig_filename + "\n";
				}
				delete motionp;
			}
			delete[] buffer;
		}
	}
	else if (exten == "bvh")
	{
		error_msg = "Bulk upload of animation files is not supported.\n";
		error_label = "DoNotSupportBulkAnimationUpload";
		error = true;
	}
	else if (exten == "gltf" || exten == "glb")
	{
		error_msg = "Bulk upload of GLTF files is not supported.\n";
		error_label = "DoNotSupportBulkGLTFUpload";
		error = true;
	}
	else if (exten == "tmp")
	{
		// This is a generic .lin resource file
		asset_type = LLAssetType::AT_OBJECT;
		LLFILE* in = LLFile::open(orig_filename, "rb");
		if (in)
		{
			// Read in the file header
			char buf[16384];
			size_t readbytes;
			S32 version;
			if (fscanf(in, "LindenResource\nversion %d\n", &version))
			{
				if (version == 2)
				{
					// NOTE: This buffer size is hard coded into scanf() below.
					char label[MAX_STRING];
					char value[MAX_STRING];
					S32  tokens_read;
					while (fgets(buf, 1024, in))
					{
						label[0] = '\0';
						value[0] = '\0';
						tokens_read = sscanf(buf, "%254s %254s\n", label,
											 value);

						llinfos << "got: " << label << " = " << value << llendl;

						if (tokens_read == EOF)
						{
							error_msg = "Corrupt resource file: " +
										orig_filename;
							error_label = "CorruptResourceFile";
							args["FILE"] = orig_filename;
							error = true;
							break;
						}
						else if (tokens_read == 2)
						{
							if (!strcmp("type", label))
							{
								asset_type = (LLAssetType::EType)(atoi(value));
							}
						}
						else if (!strcmp("_DATA_", label))
						{
							// Below is the data section
							break;
						}
						// Other values are currently discarded
					}
				}
				else
				{
					error_msg =
						"Unknown linden resource file version in file: " +
						 orig_filename;
					error_label = "UnknownResourceFileVersion";
					args["FILE"] = orig_filename;
					error = true;
				}
			}
			else
			{
				// This is an original binary formatted .lin file; start over
				// at the beginning of the file
				fseek(in, 0, SEEK_SET);

				constexpr S32 MAX_ASSET_DESCRIPTION_LENGTH = 256;
				constexpr S32 MAX_ASSET_NAME_LENGTH = 64;
				S32 header_size = 34 + MAX_ASSET_DESCRIPTION_LENGTH +
								  MAX_ASSET_NAME_LENGTH;
				// Read in and throw out most of the header except for the type
				if (fread(buf, header_size, 1, in) != 1)
				{
					llwarns << "Short read" << llendl;
				}
				S16 type_num;
				memcpy(&type_num, buf + 16, sizeof(S16));
				asset_type = (LLAssetType::EType)type_num;
			}

			if (!error)
			{
				// Copy the file's data segment into another file for uploading
				LLFILE* out = LLFile::open(filename, "wb");
				if (out)
				{
					while ((readbytes = fread(buf, 1, 16384, in)))
					{
						if (fwrite(buf, 1, readbytes, out) != readbytes)
						{
							llwarns << "Short write" << llendl;
						}
					}
					LLFile::close(out);
				}
				else
				{
					error_msg = "Unable to create temporary file: " + filename;
					error_label = "UnableToCreateOutputFile";
					args["FILE"] = filename;
					error = true;
				}
			}
			LLFile::close(in);
		}
		else
		{
			llinfos << "Could not open .lin file " << orig_filename << llendl;
		}
	}
	else	// Unknown extension
	{
		error_msg = "Unsupported file extension ." + exten + "\n";
		error_msg += "Expected .wav, .tga, .bmp, .jpg, .jpeg, .bvh or .anim";
		error = true;
	}

	LLSD result(LLSD::emptyMap());

	if (error)
	{
		result["error"] = LLSD::Binary(true);
		result["message"] = error_msg;
		result["label"] = error_label;
		result["args"] = args;
	}
	else
	{
		setAssetType(asset_type);

		// Copy this file into the cache for upload
		S64 file_size;
		LLFile infile(filename, "rb", &file_size);
		if (infile)
		{
			LLFileSystem file(getAssetId(), LLFileSystem::APPEND);

			constexpr S32 buf_size = 65536;
			U8 copy_buf[buf_size];
			while ((file_size = infile.read(copy_buf, buf_size)))
			{
				file.write(copy_buf, file_size);
			}
		}
		else
		{
			error_msg = "Unable to access temporary file: " + filename;
			result["error"] = LLSD::Binary(true);
			result["message"] = error_msg;
		}
	}

	if (!LLFile::remove(filename))
	{
		llwarns << "Unable to remove temporary file: " << filename << llendl;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// LLNewBufferedResourceUploadInfo class
///////////////////////////////////////////////////////////////////////////////
LLNewBufferedResourceUploadInfo::LLNewBufferedResourceUploadInfo(
		const std::string& buffer, const LLAssetID& asset_id,
		const std::string& name, const std::string& description,
		S32 compression_info, LLFolderType::EType dest_type,
		LLInventoryType::EType inv_type, LLAssetType::EType asset_type,
		U32 perms_next, U32 perms_group, U32 perms_everyone, S32 cost,
		uploaded_cb_t finish, failed_cb_t failure)
:	LLResourceUploadInfo(name, description, compression_info, dest_type,
						 inv_type, perms_next, perms_group,perms_everyone,
						 cost),
	mBuffer(buffer),
	mFinishFn(finish),
	mFailureFn(failure)
{
	setAssetType(asset_type);
	setAssetId(asset_id);
}

//virtual
LLSD LLNewBufferedResourceUploadInfo::prepareUpload()
{
	if (getAssetId().isNull())
	{
		generateNewAssetId();
	}

	LLSD result = exportTempFile();
	if (result.has("error"))
	{
		return result;
	}

	return LLResourceUploadInfo::prepareUpload();
}

//virtual
LLSD LLNewBufferedResourceUploadInfo::exportTempFile()
{
	std::string filename = gDirUtilp->getTempFilename();
	LLFileSystem file(getAssetId(), LLFileSystem::APPEND);
	file.write((U8*)mBuffer.c_str(), mBuffer.size());
	return LLSD();
}

//virtual
LLUUID LLNewBufferedResourceUploadInfo::finishUpload(const LLSD& result)
{
	LLUUID new_asset_id = LLResourceUploadInfo::finishUpload(result);

	if (mFinishFn)
	{
		mFinishFn(result["new_asset"].asUUID(), result);
	}

	return new_asset_id;
}

void LLNewBufferedResourceUploadInfo::failedUpload(const LLSD& result,
												   std::string& reason)
{
	if (!mFailureFn.empty())
	{
		mFailureFn(getAssetId(), result, reason);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLBufferedAssetUploadInfo class
///////////////////////////////////////////////////////////////////////////////

LLBufferedAssetUploadInfo::LLBufferedAssetUploadInfo(const LLUUID& item_id,
													 LLAssetType::EType atype,
													 const std::string& buffer,
													 inv_uploaded_cb_t finish,
													 failed_cb_t failed)
:	LLResourceUploadInfo(LLStringUtil::null, LLStringUtil::null, 0,
						 LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
						 0, 0, 0, 0),
	mTaskUpload(false),
	mContents(buffer),
	mInvnFinishFn(finish),
	mTaskFinishFn(NULL),
	mFailureFn(failed),
	mStoredToCache(false)
{
	setItemId(item_id);
	setAssetType(atype);
	setShowInventoryPanel(false);
}

LLBufferedAssetUploadInfo::LLBufferedAssetUploadInfo(const LLUUID& item_id,
													 LLPointer<LLImageFormatted> image,
													 inv_uploaded_cb_t finish)
:	LLResourceUploadInfo(LLStringUtil::null, LLStringUtil::null, 0,
						 LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
						 0, 0, 0, 0),
	mTaskUpload(false),
	mInvnFinishFn(finish),
	mTaskFinishFn(NULL),
	mFailureFn(NULL),
	mStoredToCache(false)
{
	setItemId(item_id);
	setShowInventoryPanel(false);

	EImageCodec codec = (EImageCodec)image->getCodec();
	switch (codec)
	{
		case IMG_CODEC_JPEG:
			setAssetType(LLAssetType::AT_IMAGE_JPEG);
			llinfos << "Upload Asset type set to JPEG." << llendl;
			break;

		case IMG_CODEC_TGA:
			setAssetType(LLAssetType::AT_IMAGE_TGA);
			llinfos << "Upload Asset type set to TGA." << llendl;
			break;

		default:
			llwarns << "Unknown codec to asset type transition: " << (S32)codec
					<< "." << llendl;
			break;
	}

	size_t image_size = image->getDataSize();
	mContents.reserve(image_size);
	mContents.assign((char*)image->getData(), image_size);
}

LLBufferedAssetUploadInfo::LLBufferedAssetUploadInfo(const LLUUID& task_id,
													 const LLUUID& item_id,
													 LLAssetType::EType atype,
													 const std::string& buffer,
													 task_uploaded_cb_t finish,
													 failed_cb_t failed)
:	LLResourceUploadInfo(LLStringUtil::null, LLStringUtil::null, 0,
						 LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
						 0, 0, 0, 0),
	mTaskUpload(true),
	mTaskId(task_id),
	mContents(buffer),
	mInvnFinishFn(NULL),
	mTaskFinishFn(finish),
	mFailureFn(failed),
	mStoredToCache(false)
{
	setItemId(item_id);
	setAssetType(atype);
	setShowInventoryPanel(false);
}

//virtual
LLSD LLBufferedAssetUploadInfo::prepareUpload()
{
	if (getAssetId().isNull())
	{
		generateNewAssetId();
	}

	LLFileSystem file(getAssetId(), LLFileSystem::APPEND);

	S32 size = mContents.length() + 1;
	file.write((U8*)mContents.c_str(), size);

	mStoredToCache = true;

	return LLSD().with("success", LLSD::Boolean(true));
}

//virtual
LLSD LLBufferedAssetUploadInfo::generatePostBody()
{
	LLSD body;

	if (!getTaskId().isNull())
	{
		body["task_id"] = getTaskId();
	}
	body["item_id"] = getItemId();

	return body;
}

//virtual
LLUUID LLBufferedAssetUploadInfo::finishUpload(const LLSD& result)
{
	LLUUID new_asset_id = result["new_asset"].asUUID();
	LLUUID item_id = getItemId();

	if (mStoredToCache)
	{
		LLFileSystem::renameFile(getAssetId(), new_asset_id);
	}

	if (mTaskUpload)
	{
		LLUUID task_id = getTaskId();

		dialog_refresh_all();

		if (mTaskFinishFn)
		{
			mTaskFinishFn(item_id, task_id, new_asset_id, result);
		}
	}
	else
	{
		LLUUID new_item_id;

		if (item_id.notNull())
		{
			LLViewerInventoryItem* item =
				(LLViewerInventoryItem*)gInventory.getItem(item_id);
			if (!item)
			{
				llwarns << "Inventory item for " << getDisplayName()
						<< " is no longer in agent inventory." << llendl;
				return new_asset_id;
			}

			// Update viewer inventory item
			LLPointer<LLViewerInventoryItem> new_item =
				new LLViewerInventoryItem(item);
			new_item->setAssetUUID(new_asset_id);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();

			new_item_id = new_item->getUUID();
			llinfos << "Inventory item " << item->getName() << " saved into "
					<< new_asset_id << llendl;
		}

		if (mInvnFinishFn)
		{
			mInvnFinishFn(item_id, new_asset_id, new_item_id, result);
		}
	}

	return new_asset_id;
}

void LLBufferedAssetUploadInfo::failedUpload(const LLSD& result,
											 std::string& reason)
{
	if (!mFailureFn.empty())
	{
		mFailureFn(getItemId(), getTaskId(), result, reason);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLScriptAssetUpload class
///////////////////////////////////////////////////////////////////////////////

LLScriptAssetUpload::LLScriptAssetUpload(const LLUUID& item_id,
										 const std::string& buffer,
										 TargetType_t target_type,
										 inv_uploaded_cb_t finish,
										 failed_cb_t failed)
:	LLBufferedAssetUploadInfo(item_id, LLAssetType::AT_LSL_TEXT, buffer,
							  finish, failed),
	mTargetType(target_type),
	mIsRunning(false)
{
}

LLScriptAssetUpload::LLScriptAssetUpload(const LLUUID& task_id,
										 const LLUUID& item_id,
										 TargetType_t target_type,
										 bool running, const LLUUID& exp_id,
										 const std::string& buffer,
										 task_uploaded_cb_t finish,
										 failed_cb_t failed)
:	LLBufferedAssetUploadInfo(task_id, item_id, LLAssetType::AT_LSL_TEXT,
							  buffer, finish, failed),
	mExerienceId(exp_id),
	mTargetType(target_type),
	mIsRunning(running)
{
}

//virtual
LLSD LLScriptAssetUpload::generatePostBody()
{
	LLSD body;
	body["item_id"] = getItemId();
	body["target"] = getTargetType() == MONO ? "mono" : "lsl2";
	if (getTaskId().notNull())
	{
		body["task_id"] = getTaskId();
		// NOTE: old code had the running flag as a BOOL (it is now a real
		// bool) and a BOOL is actually an S32 (which translates into an
		// LLSD::Integer instead of an LLSD::Boolean)... OpenSim expects the
		// LLSD for is_script_running to be an Integer, while SL's servers
		// accept either an Integer or a Boolean. For compatibility with
		// OpenSim, let's pass is_script_running as an LLSD integer. HB
		body["is_script_running"] = (LLSD::Integer)getIsRunning();
		body["experience"] = getExerienceId();
	}
	return body;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerAssetUpload class
///////////////////////////////////////////////////////////////////////////////

//static
LLUUID LLViewerAssetUpload::enqueueInventoryUpload(const std::string& url,
												   const LLResourceUploadInfo::ptr_t& info)
{
	std::string name = "LLViewerAssetUpload::assetInventoryUploadCoproc(";
	name += LLAssetType::lookup(info->getAssetType());
	name += ")";

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	LLUUID id =
		cpmgr->enqueueCoprocedure("Upload", name,
								  boost::bind(&LLViewerAssetUpload::assetInventoryUploadCoproc,
											  _1, url, info));
	return id;
}

//static
void LLViewerAssetUpload::assetInventoryUploadCoproc(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
													 std::string url,
													 LLResourceUploadInfo::ptr_t info)
{
	if (!adapter || !info) return;	// Paranoia

	LLSD result = info->prepareUpload();
	info->logPreparedUpload();
	if (result.has("error"))
	{
		handleUploadError(gStatusInternalError, result, info);
		return;
	}

	// Why is this here ???
	llcoro::suspend();

	if (info->showUploadDialog())
	{
		std::string upload_message = "Uploading...\n\n";
		upload_message.append(info->getDisplayName());
		LLUploadDialog::modalUploadDialog(upload_message);
	}

	LLCore::HttpOptions::ptr_t httpopt(new LLCore::HttpOptions);
	httpopt->setTimeout(LL_ASSET_UPLOAD_TIMEOUT_SEC);

	LLSD body = info->generatePostBody();

	result = adapter->postAndSuspend(url, body, httpopt);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || result.has("error"))
	{
		handleUploadError(status, result, info);
		if (info->showUploadDialog())
		{
			LLUploadDialog::modalUploadFinished();
		}
		return;
	}

	std::string uploader = result["uploader"].asString();

	if (!uploader.empty() && info->getAssetId().notNull())
	{
		result = adapter->postFileAndSuspend(uploader, info->getAssetId(),
											 info->getAssetType(), httpopt);
		status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (!status || result["state"].asString() != "complete")
		{
			handleUploadError(status, result, info);
			if (info->showUploadDialog())
			{
				LLUploadDialog::modalUploadFinished();
			}
			return;
		}
		// At this point 'status' is OK and "complete" is here, so it is a
		// success: mark it as such for the callback, if "success" is missing.
		if (!result.has("success"))
		{
			result["success"] = LLSD::Boolean(true);
		}

		S32 upload_price = result["upload_price"].asInteger();
		if (upload_price > 0)
		{
			// This upload costed us L$: update our balance and display
			// something saying that it cost L$
			LLStatusBar::sendMoneyBalanceRequest();
#if 0		// This should be useless: the balance request will trigger an
			// appropriate server-triggered notification if actually needed...
			LLSD args;
			args["AMOUNT"] = llformat("%d", uploadPrice);
			LLNotificationsUtil::add("UploadPayment", args);
#endif
		}
	}
	else
	{
		llwarns << "No upload url provided. Nothing uploaded, responding with previous result."
				<< llendl;
	}

	LLUUID new_inv_item_id = info->finishUpload(result);

	if (info->showInventoryPanel())
	{
		if (new_inv_item_id.notNull())
		{
			// Show the preview panel for textures and sounds to let the user
			// know that the image (or snapshot) arrived intact.
			LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
			if (inv)
			{
				LLFocusableElement* focus = gFocusMgr.getKeyboardFocus();
				inv->getPanel()->setSelection(new_inv_item_id, TAKE_FOCUS_NO);

				LLAssetType::EType asset_type = info->getAssetType();
				if ((asset_type == LLAssetType::AT_TEXTURE &&
					 LLPreviewTexture::getPreviewCount() < MAX_PREVIEWS) ||
					(asset_type == LLAssetType::AT_SOUND &&
					 LLPreviewSound::getPreviewCount() < MAX_PREVIEWS))
				{
					inv->getPanel()->openSelected();
				}

				// Restore keyboard focus
				gFocusMgr.setKeyboardFocus(focus);
			}
		}
		else
		{
			llwarns << "Cannot find a folder to put it in" << llendl;
		}
	}

	// Remove the "Uploading..." message
	if (info->showUploadDialog())
	{
		LLUploadDialog::modalUploadFinished();
	}

	info->performCallback(result);
}

//static
void LLViewerAssetUpload::handleUploadError(LLCore::HttpStatus status,
											const LLSD& result,
											LLResourceUploadInfo::ptr_t& info)
{
	llwarns << ll_pretty_print_sd(result) << llendl;

	LLSD args;
	if (result.has("args"))
	{
		args = result["args"];
	}

	std::string reason;
	if (result.has("message"))
	{
		reason = result["message"].asString();
	}
	else
	{
		switch (status.getType())
		{
			case 404:
				reason = LLTrans::getString("ServerUnreachable");
				break;

			case 499:
				reason = LLTrans::getString("ServerDifficulties");
				break;

			case 503:
				reason = LLTrans::getString("ServerUnavailable");
				break;

			default:
				reason = LLTrans::getString("UploadRequestInvalid");
		}
	}

	std::string label;
	if (result.has("label"))
	{
		label = result["label"].asString();
		if (label == "ErrorMessage")
		{
			args["ERROR_MESSAGE"] = reason;
		}
	}
	if (label.empty())
	{
		label = "CannotUploadReason";
		args["FILE"] = info->getDisplayName();
		args["REASON"] = reason;
	}

	gNotifications.add(label, args);

	info->failedUpload(result, reason);

	// Clear any remaining queued bulk upload assets
	gUploadQueueMutex.lock();
	gUploadQueue.clear();
	gUploadQueueMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////
// Global utlility functions for uploading assets

// This is called each time an upload happened via upload_new_resource(),
// unless an user-callback was specified. Also used in llviewermenu.cpp to
// initiate bulk uploads.
void process_bulk_upload_queue(const LLSD& result, void* userdata)
{
	gUploadQueueMutex.lock();
	if (gUploadQueue.empty())
	{
		gUploadQueueMutex.unlock();
		return;
	}

	std::string next_file = gUploadQueue.front();
	gUploadQueue.pop_front();
	gUploadQueueMutex.unlock();
	if (next_file.empty())
	{
		return;
	}

	std::string asset_name = gDirUtilp->getBaseFileName(next_file, true);
	LLStringUtil::replaceNonstandardASCII(asset_name, '?');
	LLStringUtil::replaceChar(asset_name, '|', '?');
	LLStringUtil::stripNonprintable(asset_name);
	LLStringUtil::trim(asset_name);

	LLResourceUploadInfo::ptr_t
		info(new LLNewFileResourceUploadInfo(next_file,
											 asset_name, asset_name, 0,
											 LLFolderType::FT_NONE,
											 LLInventoryType::IT_NONE,
											 LLFloaterPerms::getNextOwnerPerms(),
											 LLFloaterPerms::getGroupPerms(),
											 LLFloaterPerms::getEveryonePerms(),
											 -1));	// Unknown upload cost
	upload_new_resource(info);
}

// Local, default callbacks
void upload_done_callback(const LLUUID& uuid, void* user_data,
						  S32 result, LLExtStat ext_status)
{
	LLResourceData* data = (LLResourceData*)user_data;
	if (data)
	{
		if (result >= 0)
		{
			LLFolderType::EType dest_loc;
			if (data->mPreferredLocation == LLFolderType::FT_NONE)
			{
				dest_loc =
					LLFolderType::assetTypeToFolderType(data->mAssetInfo.mType);
			}
			else
			{
				dest_loc = data->mPreferredLocation;
			}

			LLAssetType::EType asset_type = data->mAssetInfo.mType;
			if (data->mExpectedUploadCost < 0)	// Unknown upload cost
			{
				data->mExpectedUploadCost =
					upload_cost_for_asset_type(asset_type);
			}
			bool is_balance_sufficient = true;
			if (asset_type == LLAssetType::AT_SOUND ||
				asset_type == LLAssetType::AT_TEXTURE ||
				asset_type == LLAssetType::AT_ANIMATION)
			{
				// Charge the user for the upload.
				LLViewerRegion* region = gAgent.getRegion();

				if (!can_afford_transaction(data->mExpectedUploadCost))
				{
					// *TODO: Translate
					LLFloaterBuyCurrency::buyCurrency(llformat("Uploading %s costs",
															   data->mAssetInfo.getName().c_str()),
															   data->mExpectedUploadCost);
					is_balance_sufficient = false;
				}
				else if (region)
				{
					// Charge user for upload
					if (gStatusBarp)
					{
						gStatusBarp->debitBalance(data->mExpectedUploadCost);
					}

					LLMessageSystem* msg = gMessageSystemp;
					msg->newMessageFast(_PREHASH_MoneyTransferRequest);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
					msg->nextBlockFast(_PREHASH_MoneyData);
					msg->addUUIDFast(_PREHASH_SourceID, gAgentID);
					msg->addUUIDFast(_PREHASH_DestID, LLUUID::null);
					msg->addU8("Flags", 0);
					// we tell the sim how much we were expecting to pay so it
					// can respond to any discrepancy
					msg->addS32Fast(_PREHASH_Amount,
									data->mExpectedUploadCost);
					msg->addU8Fast(_PREHASH_AggregatePermNextOwner,
								   (U8)LLAggregatePermissions::AP_EMPTY);
					msg->addU8Fast(_PREHASH_AggregatePermInventory,
								   (U8)LLAggregatePermissions::AP_EMPTY);
					msg->addS32Fast(_PREHASH_TransactionType,
									TRANS_UPLOAD_CHARGE);
					msg->addStringFast(_PREHASH_Description, NULL);
					msg->sendReliable(region->getHost());
				}
			}
			if (is_balance_sufficient)
			{
				// Actually add the upload to inventory
				llinfos << "Adding " << uuid << " to inventory." << llendl;
				const LLUUID folder_id =
					gInventory.findChoosenCategoryUUIDForType(dest_loc);
				if (folder_id.notNull())
				{
					U32 next_owner_perms = data->mNextOwnerPerm;
					if (next_owner_perms == PERM_NONE)
					{
						next_owner_perms = PERM_MOVE | PERM_TRANSFER;
					}
					create_inventory_item(folder_id,
										  data->mAssetInfo.mTransactionID,
										  data->mAssetInfo.getName(),
										  data->mAssetInfo.getDescription(),
										  data->mAssetInfo.mType,
										  data->mInventoryType,
										  NO_INV_SUBTYPE, next_owner_perms);
				}
				else
				{
					llwarns << "Cannot find an adequate inventory folder for: "
							<< data->mAssetInfo.getName() << llendl;
				}
			}
		}
		else
		{
			LLSD args;
			args["FILE"] =
				LLInventoryType::lookupHumanReadable(data->mInventoryType);
			args["REASON"] =
				std::string(LLAssetStorage::getErrorString(result));
			gNotifications.add("CannotUploadReason", args);
		}

		delete data;
		data = NULL;
	}

	LLUploadDialog::modalUploadFinished();

	process_bulk_upload_queue();
}

void temp_upload_done_callback(const LLUUID& uuid, void* user_data,
							   S32 result, LLExtStat ext_status)
{
	LLResourceData* data = (LLResourceData*)user_data;
	if (data && result >= 0)
	{
		LLFolderType::EType dest_loc =
			data->mPreferredLocation == LLFolderType::FT_NONE ?
				LLFolderType::assetTypeToFolderType(data->mAssetInfo.mType) :
				data->mPreferredLocation;
		LLUUID folder_id(gInventory.findChoosenCategoryUUIDForType(dest_loc));
		LLUUID item_id;
		item_id.generate();
		LLPermissions perm;
		perm.init(gAgentID, gAgentID, gAgentID, gAgentID);
		perm.setMaskBase(PERM_ALL);
		perm.setMaskOwner(PERM_ALL);
		perm.setMaskEveryone(PERM_ALL);
		perm.setMaskGroup(PERM_ALL);
		LLPointer<LLViewerInventoryItem> item =
			new LLViewerInventoryItem(item_id, folder_id, perm,
									  data->mAssetInfo.mTransactionID.makeAssetID(gAgent.getSecureSessionID()),
									  data->mAssetInfo.mType,
									  data->mInventoryType,
									  data->mAssetInfo.getName(),
									  "Temporary asset", LLSaleInfo::DEFAULT,
									  LLInventoryItem::II_FLAGS_NONE,
									  time_corrected());
		item->updateServer(true);
		gInventory.updateItem(item);
		gInventory.notifyObservers();
		open_texture(item_id, std::string("Texture: ") + item->getName(), true,
					 LLUUID::null, false);
	}
	else
	{
		LLSD args;
		args["FILE"] =
			LLInventoryType::lookupHumanReadable(data->mInventoryType);
		args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
		gNotifications.add("CannotUploadReason", args);
	}

	LLUploadDialog::modalUploadFinished();
	delete data;
}

void upload_new_resource(LLResourceUploadInfo::ptr_t& info,
						 LLAssetStorage::LLStoreAssetCallback callback,
						 void* userdata, bool temp_upload)
{
	if (gDisconnected || !info)
	{
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("NewFileAgentInventory");
	if (!url.empty() && !temp_upload)
	{
		llinfos << "New agent inventory via capability" << llendl;
		if (!info->hasCapCallback())
		{
			info->setCapCallback(process_bulk_upload_queue, NULL);
		}
		LLViewerAssetUpload::enqueueInventoryUpload(url, info);
	}
	else
	{
		info->prepareUpload();
		info->logPreparedUpload();

		S32 expected_upload_cost = info->getExpectedUploadCost();
		LLAssetType::EType asset_type = info->getAssetType();

		if (!temp_upload)
		{
			llinfos << "NewAgentInventory capability not found, new agent inventory via asset system."
					<< llendl;
			// Check for adequate funds. *TODO: do this check on the sim.
			if (asset_type == LLAssetType::AT_SOUND ||
				asset_type == LLAssetType::AT_TEXTURE ||
				asset_type == LLAssetType::AT_ANIMATION)
			{
				S32 balance = gStatusBarp->getBalance();
				if (balance < expected_upload_cost)
				{
					// Insufficient funds, bail on this upload
					LLFloaterBuyCurrency::buyCurrency("Uploading costs",
													  expected_upload_cost);
					return;
				}
			}
		}
		else
		{
			info->setName("[temp] " + info->getName());
			llinfos << "Uploading " << info->getName()
					<< " as a temporary (baked) texture via the asset system."
					<< llendl;
		}

		LLResourceData* data = new LLResourceData;
		data->mAssetInfo.mTransactionID = info->getTransactionId();
		data->mAssetInfo.mUuid = info->getAssetId();
		data->mAssetInfo.mType = asset_type;
		data->mAssetInfo.mCreatorID = gAgentID;
		data->mInventoryType = info->getInventoryType();
		data->mNextOwnerPerm = info->getNextOwnerPerms();
		data->mExpectedUploadCost = expected_upload_cost;
		data->mUserData = userdata;
		data->mAssetInfo.setName(info->getName());
		data->mAssetInfo.setDescription(info->getDescription());
		data->mPreferredLocation = info->getDestinationFolderType();

		LLAssetStorage::LLStoreAssetCallback asset_callback;
		asset_callback = temp_upload ? &temp_upload_done_callback
									 : &upload_done_callback;
		if (callback)
		{
			asset_callback = callback;
		}
		gAssetStoragep->storeAssetData(data->mAssetInfo.mTransactionID,
									   data->mAssetInfo.mType,
									   asset_callback, (void*)data,
									   temp_upload, true, temp_upload);
	}
}

void on_new_single_inventory_upload_complete(LLAssetType::EType asset_type,
											 LLInventoryType::EType inv_type,
											 const std::string inv_type_str,
											 const LLUUID& item_folder_id,
											 const std::string& item_name,
											 const std::string& item_description,
											 const LLSD& response,
											 S32 upload_price)
{
	if (upload_price > 0)
	{
		// this upload costed us L$, update our balance and display something
		// saying that it cost L$
		LLStatusBar::sendMoneyBalanceRequest();

		LLSD args;
		args["AMOUNT"] = llformat("%d", upload_price);
		gNotifications.add("UploadDone", args);
	}

	if (item_folder_id.notNull())
	{
		U32 everyone_perms = PERM_NONE;
		U32 group_perms = PERM_NONE;
		U32 next_owner_perms = PERM_ALL;
		if (response.has("new_next_owner_mask"))
		{
			// The server provided creation perms so use them. Do not assume we
			// got the perms we asked for since the server may not have granted
			// them all.
			everyone_perms = response["new_everyone_mask"].asInteger();
			group_perms = response["new_group_mask"].asInteger();
			next_owner_perms = response["new_next_owner_mask"].asInteger();
		}
		else
		{
			// The server does not provide creation perms, so use tha old
			// assumption-based perms.
			if (inv_type_str != "snapshot")
			{
				next_owner_perms = PERM_MOVE | PERM_TRANSFER;
			}
		}

		LLPermissions new_perms;
		new_perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		new_perms.initMasks(PERM_ALL, PERM_ALL, everyone_perms, group_perms,
							next_owner_perms);

		U32 inv_item_flags = 0;
		if (response.has("inventory_flags"))
		{
			inv_item_flags = response["inventory_flags"].asInteger();
			if (inv_item_flags)
			{
				llinfos << "Inventory item flags: " << inv_item_flags
						<< llendl;
			}
		}
		S32 creation_date_now = time_corrected();
		LLPointer<LLViewerInventoryItem> item =
			new LLViewerInventoryItem(response["new_inventory_item"].asUUID(),
									  item_folder_id, new_perms,
									  response["new_asset"].asUUID(),
									  asset_type, inv_type, item_name,
									  item_description, LLSaleInfo::DEFAULT,
									  inv_item_flags, creation_date_now);

		gInventory.updateItem(item);
		gInventory.notifyObservers();

		// Show the preview panel for textures and sounds to let user know that
		// the image (or snapshot) arrived intact.
		LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
		if (inv)
		{
			LLFocusableElement* focus = gFocusMgr.getKeyboardFocus();

			inv->getPanel()->setSelection(response["new_inventory_item"].asUUID(),
										  TAKE_FOCUS_NO);
			if ((LLAssetType::AT_TEXTURE == asset_type &&
				 LLPreviewTexture::getPreviewCount() < MAX_PREVIEWS) ||
				(LLAssetType::AT_SOUND == asset_type &&
				 LLPreviewSound::getPreviewCount() < MAX_PREVIEWS))
			{
				inv->getPanel()->openSelected();
			}

			// Restore keyboard focus
			gFocusMgr.setKeyboardFocus(focus);
		}
	}
	else
	{
		llwarns << "Cannot find a folder to put '" << item_name << "' into."
				<< llendl;
	}

	// Remove the "Uploading..." message
	LLUploadDialog::modalUploadFinished();
}
