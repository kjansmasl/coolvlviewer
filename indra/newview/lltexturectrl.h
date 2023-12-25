/**
 * @file lltexturectrl.h
 * @author Richard Nelson, James Cook
 * @brief LLTextureCtrl class header file including related functions
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

#ifndef LL_LLTEXTURECTRL_H
#define LL_LLTEXTURECTRL_H

#include "llcoord.h"
#include "llfloater.h"
#include "llpermissionsflags.h"
#include "llstring.h"
#include "lluictrl.h"

class LLFloaterTexturePicker;
class LLInventoryItem;
class LLTextBox;
class LLViewBorder;
class LLViewerFetchedTexture;

// Used for setting drag & drop callbacks.
typedef bool (*drag_n_drop_callback)(LLUICtrl*, LLInventoryItem*, void*);

class LLTextureCtrl final : public LLUICtrl
{
protected:
	LOG_CLASS(LLTextureCtrl);

public:
	typedef enum e_texture_pick_op
	{
		TEXTURE_CHANGE,
		TEXTURE_SELECT,
		TEXTURE_CANCEL
	} ETexturePickOp;

public:
	LLTextureCtrl(const std::string& name, const LLRect& rect,
				  const std::string& label, const LLUUID& image_id,
				  const LLUUID& default_image_id,
				  const std::string& default_image_name);
	~LLTextureCtrl() override;

	// LLView interface
	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;

	void setVisible(bool visible) override;
	void setEnabled(bool enabled) override;

	void draw() override;

	LL_INLINE bool isDirty() const override					{ return mDirty; }
	LL_INLINE void resetDirty() override					{ mDirty = false; }

	// LLUICtrl interface
	void clear() override;

	// Takes a UUID, wraps get/setImageAssetID
	void setValue(const LLSD& value) override;
	LLSD getValue() const override;

	// LLTextureCtrl interface

	void setValid(bool valid);

	void showPicker(bool take_focus);
	LL_INLINE bool isPickerShown()							{ return !mFloaterHandle.isDead(); }

	void setLabel(const std::string& label);
	LL_INLINE const std::string& getLabel() const			{ return mLabel; }

	LL_INLINE void setAllowNoTexture(bool b)				{ mAllowNoTexture = b; }
	LL_INLINE bool getAllowNoTexture() const				{ return mAllowNoTexture; }

	LL_INLINE void setAllowInvisibleTexture(bool b)			{ mAllowInvisibleTexture = b; }
	LL_INLINE bool getAllowInvisibleTexture() const			{ return mAllowInvisibleTexture; }

	LL_INLINE void setAllowLocalTexture(bool b)				{ mAllowLocalTexture = b; }
	LL_INLINE bool getAllowLocalTexture() const				{ return mAllowLocalTexture; }

	LL_INLINE const LLUUID& getImageItemID()				{ return mImageItemID; }

	void setImageAssetID(const LLUUID& image_asset_id);
	LL_INLINE const LLUUID& getImageAssetID() const			{ return mImageAssetID; }

	LL_INLINE void setDefaultImageAssetID(const LLUUID& id)	{ mDefaultImageAssetID = id; }
	LL_INLINE const LLUUID& getDefaultImageAssetID() const	{ return mDefaultImageAssetID; }

	LL_INLINE void setBlankImageAssetID(const LLUUID& id)	{ mBlankImageAssetID = id; }
	LL_INLINE const LLUUID& getBlankImageAssetID() const	{ return mBlankImageAssetID; }

	LL_INLINE const std::string& getDefaultImageName() const
	{
		return mDefaultImageName;
	}

	void setFallbackImageName(const std::string& name);

	void setCaption(const std::string& caption);

	LL_INLINE void setCaptionAlwaysEnabled(bool b = true)	{ mCaptionAlwaysEnabled = b; }

	void setCanApplyImmediately(bool b);
	void setBakeTextureEnabled(bool b);

	void setImmediateFilterPermMask(PermissionMask mask);

	LL_INLINE void setNonImmediateFilterPermMask(PermissionMask mask)
	{
		mNonImmediateFilterPermMask = mask;
	}

	LL_INLINE PermissionMask getImmediateFilterPermMask()
	{
		return mImmediateFilterPermMask;
	}

	LL_INLINE PermissionMask getNonImmediateFilterPermMask()
	{
		return mNonImmediateFilterPermMask;
	}

	void closeFloater();

	void onFloaterClose();
	void onFloaterCommit(ETexturePickOp op, const LLUUID& id = LLUUID::null,
						 const LLUUID& tracking_id = LLUUID::null);

	// This call is returned when a drag is detected. Your callback should
	// return true if the drag is acceptable.
	LL_INLINE void setDragCallback(drag_n_drop_callback cb)	{ mDragCallback = cb; }

	// This callback is called when the drop happens. Return true if the drop
	// happened - resulting in an on commit callback, but not necessariliy any
	// other change.
	LL_INLINE void setDropCallback(drag_n_drop_callback cb)	{ mDropCallback = cb; }

	LL_INLINE void setOnCancelCallback(LLUICtrlCallback cb)	{ mOnCancelCallback = cb; }

	LL_INLINE void setOnCloseCallback(LLUICtrlCallback cb)	{ mOnCloseCallback = cb; }

	LL_INLINE void setOnSelectCallback(LLUICtrlCallback cb)	{ mOnSelectCallback = cb; }

	LL_INLINE void setShowLoadingPlaceholder(bool b)		{ mShowLoadingPlaceholder = b; }

	LL_INLINE void setDisplayRatio(F32 ratio)				{ mDisplayRatio = ratio; }

	LL_INLINE bool isImageLocal() const						{ return mLocalTrackingID.notNull(); }
	LL_INLINE const LLUUID& getLocalTrackingID() const		{ return mLocalTrackingID; }

private:
	bool allowDrop(LLInventoryItem* item);
	bool doDrop(LLInventoryItem* item);

private:
	LLHandle<LLFloater>					mFloaterHandle;

	drag_n_drop_callback				mDragCallback;
	drag_n_drop_callback				mDropCallback;

	LLUICtrlCallback					mOnCancelCallback;
	LLUICtrlCallback					mOnCloseCallback;
	LLUICtrlCallback					mOnSelectCallback;

	LLPointer<LLViewerFetchedTexture>	mTexturep;
	// What to show if currently selected texture is null:
	LLPointer<LLViewerFetchedTexture>	mFallbackImagep;

	LLColor4							mBorderColor;

	LLUUID								mImageItemID;
	LLUUID								mImageAssetID;
	LLUUID								mDefaultImageAssetID;
	LLUUID								mBlankImageAssetID;
	LLUUID								mLocalTrackingID;

	LLTextBox*							mCaption;
	LLViewBorder*						mBorder;
	LLTextBox*							mTentativeLabel;

	std::string							mFallbackImageName;
	std::string							mDefaultImageName;
	std::string							mLabel;
	LLWString							mLoadingPlaceholderString;

	LLCoordGL							mLastFloaterLeftTop;

	F32									mDisplayRatio;

	PermissionMask						mImmediateFilterPermMask;
	PermissionMask						mNonImmediateFilterPermMask;

	// If true, the user can select "none" as an option:
	bool								mAllowNoTexture;
	// If true, the user can select "Invisible" as an option:
	bool								mAllowInvisibleTexture;
	bool								mCanApplyImmediately;
	bool								mAllowLocalTexture;
	bool								mValid;
	bool								mDirty;
	bool								mEnabled;
	bool								mCaptionAlwaysEnabled;
	bool								mShowLoadingPlaceholder;
	bool								mBakeTextureEnabled;
};

#endif  // LL_LLTEXTURECTRL_H
