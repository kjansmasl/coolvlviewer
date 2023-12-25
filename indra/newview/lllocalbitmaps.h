/**
 * @file lllocalbitmaps.h
 * @author Vaalith Jinn, code cleanup by Henri Beauchamp
 * @brief Local Bitmaps header
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Linden Research, Inc.
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

#ifndef LL_LOCALBITMAPS_H
#define LL_LOCALBITMAPS_H

#include <list>
#include <time.h>						// For time_t

#include "boost/signals2.hpp"

#include "llavatarappearancedefines.h"
#include "lleventtimer.h"
#include "hbfileselector.h"
#include "llimage.h"
#include "llpointer.h"
#include "llwearabletype.h"

class LLGLTFMaterial;
class LLViewerObject;

class LLLocalBitmapTimer final : public LLEventTimer
{
public:
	LLLocalBitmapTimer();

	bool tick() override;

	void startTimer();
	void stopTimer();
	bool isRunning();
};

class LLLocalBitmap
{
protected:
	LOG_CLASS(LLLocalBitmap);

public:
	LLLocalBitmap(std::string filename);
	~LLLocalBitmap();

	LL_INLINE const std::string& getFilename() const	{ return mFilename; }
	LL_INLINE const std::string& getShortName() const	{ return mShortName; }
	LL_INLINE const LLUUID& getTrackingID()	const		{ return mTrackingID; }
	LL_INLINE const LLUUID& getWorldID() const			{ return mWorldID; }
	LL_INLINE bool getValid() const						{ return mValid; }

	enum EUpdateType
	{
		UT_FIRSTUSE,
		UT_REGUPDATE
	};

	bool updateSelf(EUpdateType = UT_REGUPDATE);

	typedef boost::signals2::signal<void(const LLUUID& tracking_id,
										 const LLUUID& old_id,
										 const LLUUID& new_id)> changed_sig_t;
	typedef changed_sig_t::slot_type changed_cb_t;
	boost::signals2::connection setChangedCallback(const changed_cb_t& cb);

	void addGLTFMaterial(LLGLTFMaterial* matp);

	typedef std::list<LLLocalBitmap*> list_t;
	LL_INLINE static const list_t& getBitmapList()		{ return sBitmapList; }

	LL_INLINE static S32 getBitmapListVersion()			{ return sBitmapsListVersion; }

	static void addUnits();
	static void delUnit(const LLUUID& tracking_id);

	static const LLUUID& getWorldID(const LLUUID& tracking_id);
	static bool isLocal(const LLUUID& world_id);
	static const std::string& getFilename(const LLUUID& tracking_id);

	static void doUpdates();
	static void setNeedsRebake();
	static void doRebake();

	static boost::signals2::connection setOnChangedCallback(const LLUUID& trac_id,
															const changed_cb_t& cb);
	static void associateGLTFMaterial(const LLUUID& tracking_id,
									  LLGLTFMaterial* matp);

	// To be called on viewer shutdown in LLAppViewer::cleanup(). HB
	static void cleanupClass();

private:
	bool decodeBitmap(LLPointer<LLImageRaw> raw);
	void replaceIDs(const LLUUID& old_id, LLUUID new_id);
	void prepUpdateObjects(const LLUUID& old_id, U32 channel,
						   std::vector<LLViewerObject*>& obj_list);
	void updateUserPrims(const LLUUID& old_id, const LLUUID& new_id,
						 U32 channel);
	void updateUserVolumes(const LLUUID& old_id, const LLUUID& new_id,
						   U32 channel);
	void updateUserLayers(const LLUUID& old_id, const LLUUID& new_id,
						  LLWearableType::EType type);
	void updateGLTFMaterials(const LLUUID& old_id, const LLUUID& new_id);
	LLAvatarAppearanceDefines::ETextureIndex
		getTexIndex(LLWearableType::EType type,
					LLAvatarAppearanceDefines::EBakedTextureIndex index);

	enum ELinkStatus
	{
		LS_ON,
		LS_BROKEN
	};

	enum EExtension
	{
		ET_IMG_BMP,
		ET_IMG_TGA,
		ET_IMG_JPG,
		ET_IMG_PNG
	};

	static void addUnitsCallback(HBFileSelector::ELoadFilter type,
								 std::deque<std::string>& files, void*);

private:
	S32							mUpdateRetries;
	EExtension					mExtension;
	ELinkStatus					mLinkStatus;
	LLUUID						mTrackingID;
	LLUUID						mWorldID;
	std::string					mFilename;
	std::string					mShortName;
	changed_sig_t				mChangedSignal;
	typedef std::vector<LLPointer<LLGLTFMaterial> > mat_list_t;
	mat_list_t					mGLTFMaterialWithLocalTextures;
	time_t						mLastModified;
	bool						mValid;

	static LLLocalBitmapTimer	sTimer;
	static list_t				sBitmapList;
	static S32					sBitmapsListVersion;
	static bool					sNeedsRebake;
};

#endif
