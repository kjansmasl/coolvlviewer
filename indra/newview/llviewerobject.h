/**
 * @file llviewerobject.h
 * @brief Description of LLViewerObject class, which is the base class for most
 * objects in the viewer.
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

#ifndef LL_LLVIEWEROBJECT_H
#define LL_LLVIEWEROBJECT_H

#include <map>

#include "llassetstorage.h"
#include "llbbox.h"
#include "llinventory.h"
#include "llprimitive.h"
#include "llvertexbuffer.h"
#include "object_flags.h"

#include "llhudicon.h"
#include "llvoinventorylistener.h"

class LLAlphaObject;
class LLAudioSource;
class LLAudioSourceVO;
class LLDataPacker;
class LLDataPackerBinaryBuffer;
class LLDrawable;
class LLGLTFMaterial;
class LLHUDText;
class LLMaterialID;
class LLNameValue;
class LLMessageSystem;
class LLPartSysData;
class LLReflectionMap;
class LLReflectionProbeParams;
class LLRenderMaterialParams;
class LLViewerFetchedTexture;
class LLViewerTexture;
class LLViewerInventoryItem;
class LLViewerObject;
class LLViewerPartSourceScript;
class LLViewerRegion;
class LLVOAvatar;
class LLVOAvatarPuppet;
class LLVOPartGroup;
class LLVOVolume;

typedef enum e_object_update_type
{
	OUT_FULL,
	OUT_TERSE_IMPROVED,
	OUT_FULL_COMPRESSED,
	OUT_FULL_CACHED,
	OUT_UNKNOWN,
} EObjectUpdateType;

// Callback typedef for inventory
typedef void (*inventory_callback)(LLViewerObject*,
								   LLInventoryObject::object_list_t*,
								   S32 serial_num, void*);

class LLViewerObjectMedia
{
public:
	LLViewerObjectMedia()
	:	mMediaURL(),
		mPassedWhitelist(false),
		mMediaType(0)
	{
	}

	std::string	mMediaURL;			// For web pages on surfaces, one per prim
	U8			mMediaType;			// See LLTextureEntry::WEB_PAGE, etc.
	bool		mPassedWhitelist;	// User has OK'd display
};

class LLViewerObject : public LLPrimitive, public LLRefCount, public LLGLUpdate
{
	friend class LLViewerMediaList;
	friend class LLViewerObjectList;

protected:
	LOG_CLASS(LLViewerObject);

	~LLViewerObject() override; // use unref()

	class LLInventoryCallbackInfo
	{
	public:
		~LLInventoryCallbackInfo();

	public:
		LLViewerObject*			mObject;
		LLVOInventoryListener*	mListener;
		void*					mInventoryData;
	};
	typedef std::list<LLInventoryCallbackInfo*> callback_list_t;

public:
	typedef std::list<LLPointer<LLViewerObject> > child_list_t;
	typedef std::list<LLPointer<LLViewerObject> > vobj_list_t;

	typedef const child_list_t const_child_list_t;

	// Viewer-side only types; use the LL_PCODE_APP mask.
	typedef enum e_vo_types
	{
		LL_VO_CLOUDS			= LL_PCODE_APP | 0x20,
		LL_VO_SURFACE_PATCH		= LL_PCODE_APP | 0x30,
		LL_VO_WL_SKY			= LL_PCODE_APP | 0x40,
		LL_VO_SQUARE_TORUS		= LL_PCODE_APP | 0x50,
		LL_VO_SKY				= LL_PCODE_APP | 0x60,
		LL_VO_VOID_WATER		= LL_PCODE_APP | 0x70,
		LL_VO_WATER				= LL_PCODE_APP | 0x80,
		LL_VO_PART_GROUP		= LL_PCODE_APP | 0xa0,
		LL_VO_TRIANGLE_TORUS	= LL_PCODE_APP | 0xb0,
		LL_VO_HUD_PART_GROUP	= LL_PCODE_APP | 0xc0,
	} EVOType;

	typedef enum e_physics_shape_types
	{
		PHYSICS_SHAPE_PRIM = 0,
		PHYSICS_SHAPE_NONE,
		PHYSICS_SHAPE_CONVEX_HULL,
	} EPhysicsShapeType;

	// Types of media we can associate
	enum { MEDIA_NONE = 0, MEDIA_SET = 1 };

	// Return codes for processUpdateMessage
	enum
	{
		MEDIA_URL_REMOVED	= 0x1,
		MEDIA_URL_ADDED		= 0x2,
		MEDIA_URL_UPDATED	= 0x4,
		MEDIA_FLAGS_CHANGED	= 0x8,
		INVALID_UPDATE		= 0x80000000
	};

	LLViewerObject(const LLUUID& id, LLPCode pcode, LLViewerRegion* regionp,
				   bool is_global = false);

	LL_INLINE virtual LLVOVolume* asVolume() 			{ return NULL; }

	// To void slow dynamic casts...
	LL_INLINE virtual LLAlphaObject* asAlphaObject()
	{
		return NULL;
	}

	LL_INLINE virtual LLVOPartGroup* asVOPartGroup()
	{
		return NULL;
	}

	// Mark this object as dead, and clean up its references
	virtual void markDead();
	LL_INLINE bool isDead() const						{ return mDead; }
	LL_INLINE bool isOrphaned() const					{ return mOrphaned; }
	bool isParticleSource() const;

	LL_INLINE virtual LLVOAvatar* asAvatar()			{ return NULL; }

	static void initVOClasses();
	static void cleanupVOClasses();

	void addNVPair(const std::string& data);
	bool removeNVPair(const std::string& name);
	// NULL if no name value pair by that name:
	LLNameValue* getNVPair(const std::string& name) const;

	// Object creation and updating methods

	virtual void idleUpdate(F64 time);

	static U32 extractSpatialExtents(LLDataPackerBinaryBuffer* dp,
									 LLVector3& pos, LLVector3& scale,
									 LLQuaternion& rot);
	virtual U32 processUpdateMessage(LLMessageSystem* mesgsys,
									 void** user_data, U32 block_num,
									 EObjectUpdateType update_type,
									 LLDataPacker* dp);

	// Whether this object needs to do an idleUpdate():
	LL_INLINE virtual bool isActive() const				{ return true; }
	LL_INLINE bool onActiveList() const					{ return mOnActiveList; }
	LL_INLINE void setOnActiveList(bool on_active)		{ mOnActiveList = on_active; }

	LL_INLINE virtual bool isAttachment() const			{ return false; }

	LL_INLINE virtual bool isTempAttachment() const
	{
		return mID.notNull() && mID == mAttachmentItemID;
	}

	const std::string& getAttachmentItemName();

	// If this object is directly or indirectly parented by an avatar, return
	// it. Normally getAvatar() is the correct method to call and it returns
	// the avatar used for skinning. The exception is with animated objects
	// that are also attachments; in that case, getAvatar() returns the puppet
	// avatar, used for skinning, and getAvatarAncestor() returns the avatar to
	// which the object is attached.
	LLVOAvatar* getAvatarAncestor();
	virtual LLVOAvatar* getAvatar() const;

	bool hasRenderMaterialParams() const;
	void setHasRenderMaterialParams(bool has_params);
	const LLUUID& getRenderMaterialID(U8 te) const;
	// Sets the render material Id for the given texture entry (or -1 for all
	// texture entries).
	void setRenderMaterialID(S32 te, const LLUUID& id,
							 bool update_server = true,
							 bool local_origin = true);
	LL_INLINE void setRenderMaterialIDs(const LLUUID& id)
	{
		setRenderMaterialID(-1, id, true);
	}

	LL_INLINE virtual bool isHUDAttachment() const		{ return false; }
	LL_INLINE virtual void updateRadius()				{}
	// default implementation is mDrawable->getRadius():
	virtual F32 getVObjRadius() const;

	// For jointed and other parent-relative hacks
	LLViewerObject* getSubParent();
	const LLViewerObject* getSubParent() const;

	// These methods return mPuppetAvatar for the edit root prim of this link
	// set
	LLVOAvatarPuppet* getPuppetAvatar();
	LLVOAvatarPuppet* getPuppetAvatar() const;

	void linkPuppetAvatar();
	void unlinkPuppetAvatar();
	// Links or unlinks as needed
	void updatePuppetAvatar();

	LL_INLINE virtual bool isAnimatedObject() const		{ return false; }

#if LL_ANIMESH_VPARAMS
	// Extended attributes, initially used for animated object visual params
	// but general mechanism.
	LLSD getVisualParamsSD() const;
	void applyExtendedAttributes();
#endif

	// Object visiblility and GPW methods

	// Override to generate accurate apparent angle and area
	virtual void setPixelAreaAndAngle();

	virtual U32 getNumVertices() const;
	virtual U32 getNumIndices() const;
	LL_INLINE S32 getNumFaces() const					{ return mNumFaces; }

	// Graphical stuff for objects - maybe broken out into render class later ?
	LL_INLINE virtual void updateTextures()				{}

	LL_INLINE virtual void faceMappingChanged()			{}

	// When you just want to boost priority of this object:
	virtual void boostTexturePriority(bool boost_children = true);

	LL_INLINE virtual LLDrawable* createDrawable()		{ return NULL; }
	LL_INLINE virtual bool updateGeometry(LLDrawable*)	{ return true; }

	LL_INLINE void updateGL() override					{}

	LL_INLINE virtual void resetVertexBuffers()			{}

	LL_INLINE virtual void updateFaceSize(S32 idx)		{}
	LL_INLINE virtual bool updateLOD()					{ return false; }

	virtual bool setDrawableParent(LLDrawable* parentp);

	// Used also in llselectmgr.cpp
	void resetRot();

	void setLineWidthForWindowSize(S32 window_width);

	// Accessor methods
	LL_INLINE LLViewerRegion* getRegion() const			{ return mRegionp; }

	LL_INLINE bool isSelected() const					{ return mUserSelected; }
	bool isAnySelected() const;
	virtual void setSelected(bool sel);

	// This sets the local id for this object, taking care as well to log it
	// when it relates to our debugged object. HB
	void setlocalID(U32 local_id);

	LL_INLINE const LLUUID& getID() const				{ return mID; }
	LL_INLINE U32 getLocalID() const					{ return mLocalID; }
	LL_INLINE U32 getCRC() const						{ return mTotalCRC; }
	LL_INLINE S32 getListIndex() const					{ return mListIndex; }
	LL_INLINE void setListIndex(S32 idx)				{ mListIndex = idx; }

	LL_INLINE virtual bool isFlexible() const			{ return false; }
	LL_INLINE virtual bool isSculpted() const 			{ return false; }
	LL_INLINE virtual bool isMesh() const				{ return false; }
	LL_INLINE virtual bool isRiggedMesh() const			{ return false; }
	LL_INLINE virtual bool hasLightTexture() const		{ return false; }
	LL_INLINE virtual bool isReflectionProbe() const	{ return false; }

	// This method returns true if the object is over land owned by the agent,
	// one of its groups, or it encroaches and anti-encroachment is enabled.
	bool isReturnable();

	struct PotentialReturnableObject
	{
		LLBBox			box;
		LLViewerRegion* region;
	};
	typedef std::vector<PotentialReturnableObject> returnable_vec_t;

	void buildReturnablesForChildrenVO(returnable_vec_t& returnables,
									   LLViewerObject* childp,
									   LLViewerRegion* target_regionp);
	void constructAndAddReturnable(returnable_vec_t& returnables,
								   LLViewerObject* childp,
								   LLViewerRegion* target_regionp);

	virtual bool setParent(LLViewerObject* parent);

	LL_INLINE virtual void onReparent(LLViewerObject* old_parent,
									  LLViewerObject* new_parent)
	{
	}

	LL_INLINE virtual void afterReparent()				{}

	virtual void addChild(LLViewerObject* childp);
	virtual void removeChild(LLViewerObject* childp);
	LL_INLINE const_child_list_t& getChildren() const	{ return mChildList; }
	LL_INLINE S32 numChildren() const					{ return mChildList.size(); }
	void addThisAndAllChildren(std::vector<LLViewerObject*>& objects);
	void addThisAndNonJointChildren(std::vector<LLViewerObject*>& objects);
	bool isChild(LLViewerObject* childp) const;
	// Returns true if at least one avatar is sitting on this object
	bool isSeat() const;
	// Returns true if agent is sitting on this object
	bool isAgentSeat() const;

	// Detects if given line segment (in agent space) intersects with this
	// object. Returns true if intersection detected and returns information
	// about the intersection.
	virtual bool lineSegmentIntersect(const LLVector4a& start,
									  const LLVector4a& end,
									  // Which face to check, -1 = ALL_SIDES
									  S32 face = -1,
									  bool pick_transparent = false,
									  bool pick_rigged = false,
									  // Which face was hit
									  S32* face_hit = NULL,
									  // Returns the intersection point
									  LLVector4a* intersection = NULL,
									  // Return the texture coordinates
									  LLVector2* tex_coord = NULL,
									  // Returns the surface normal
									  LLVector4a* normal = NULL,
									  // Returns the surface tangent
									  LLVector4a* tangent = NULL);

	virtual bool lineSegmentBoundingBox(const LLVector4a& start,
										const LLVector4a& end);

	virtual const LLVector3d getPositionGlobal() const;
	virtual const LLVector3& getPositionRegion() const;
	virtual const LLVector3 getPositionEdit() const;
	virtual const LLVector3& getPositionAgent() const;
	virtual const LLVector3 getRenderPosition() const;

	// Usually = to getPositionAgent, unless, like for flex objects, it is not:
	LL_INLINE virtual const LLVector3 getPivotPositionAgent() const
	{
		return getRenderPosition();
	}

	LLViewerObject* getRootEdit() const;

	const LLQuaternion getRotationRegion() const;
	const LLQuaternion getRotationEdit() const;
	const LLQuaternion getRenderRotation() const;
	virtual	const LLMatrix4& getRenderMatrix() const;

	// Note: this used to be setPosition() in LL's code, but there is also a
	// setPosition(const LLVector3& pos) method in LLXForm, which we derivate
	// from, via LLPrimitive... Since "damped" can be omitted here, this means
	// we could end up calling the wrong setPosition() method !  HB
	void setPositionLocal(const LLVector3& pos, bool damped = false);

	void setPositionGlobal(const LLVector3d& position, bool damped = false);
	void setPositionRegion(const LLVector3& position);
	void setPositionEdit(const LLVector3& position, bool damped = false);
	void setPositionAgent(const LLVector3& pos_agent);
	void setPositionParent(const LLVector3& pos_parent, bool damped = false);
	void setPositionAbsoluteGlobal(const LLVector3d& pos_global);

	LL_INLINE virtual const LLMatrix4& getWorldMatrix(LLXformMatrix* xfm) const
	{
		return xfm->getWorldMatrix();
	}

	LL_INLINE void setRotation(F32 x, F32 y, F32 z, bool damped = false)
	{
		LLPrimitive::setRotation(x, y, z);
		setChanged(ROTATED | SILHOUETTE);
		updateDrawable(damped);
	}

	LL_INLINE void setRotation(const LLQuaternion& quat, bool damped = false)
	{
		LLPrimitive::setRotation(quat);
		setChanged(ROTATED | SILHOUETTE);
		updateDrawable(damped);
	}

	LLViewerTexture* getBakedTextureForMagicId(const LLUUID& id);
	void updateAvatarMeshVisibility(const LLUUID& id, const LLUUID& old_id);
	void refreshBakeTexture();

	void setNumTEs(U8 num_tes) override;
	void setTE(U8 te, const LLTextureEntry& tex_entry) override;
	S32 setTETexture(U8 te, const LLUUID& uuid) override;
	S32 setTENormalMap(U8 te, const LLUUID& uuid);
	S32 setTESpecularMap(U8 te, const LLUUID& uuid);

	S32 setTETextureCore(U8 te, LLViewerTexture* texp);
	S32 setTENormalMapCore(U8 te, LLViewerTexture* texp);
	S32 setTESpecularMapCore(U8 te, LLViewerTexture* texp);

	S32 setTEColor(U8 te, const LLColor3& color) override;
	S32 setTEColor(U8 te, const LLColor4& color) override;
	S32 setTEScale(U8 te, F32 s, F32 t) override;
	S32 setTEScaleS(U8 te, F32 s) override;
	S32 setTEScaleT(U8 te, F32 t) override;
	S32 setTEOffset(U8 te, F32 s, F32 t) override;
	S32 setTEOffsetS(U8 te, F32 s) override;
	S32 setTEOffsetT(U8 te, F32 t) override;
	S32 setTERotation(U8 te, F32 r) override;
	S32 setTEBumpmap(U8 te, U8 bump) override;
	S32 setTETexGen(U8 te, U8 texgen) override;
	// *FIXME: this confusingly acts upon a superset of setTETexGen's flags
	// without absorbing its semantics:
	S32 setTEMediaTexGen(U8 te, U8 media) override;
	S32 setTEShiny(U8 te, U8 shiny) override;
	S32 setTEFullbright(U8 te, U8 fullbright) override;
	S32 setTEMediaFlags(U8 te, U8 media_flags) override;
	S32 setTEGlow(U8 te, F32 glow) override;
	S32 setTEMaterialID(U8 te, const LLMaterialID& matidp) override;
	S32 setTEMaterialParams(U8 te, const LLMaterialPtr p) override;
	virtual S32 setTEGLTFMaterialOverride(U8 te, LLGLTFMaterial* matp);

	void updateTEMaterialTextures(U8 te);

	// Used by materials update methods to properly kick off rebuilds of VBs
	// etc when materials updates require changes.
	void refreshMaterials();

	bool setMaterial(U8 material) override;

	// Not derived from LLPrimitive:
	virtual void setTEImage(U8 te, LLViewerTexture* texp);

	virtual void changeTEImage(S32 index, LLViewerTexture* texp);
	virtual void changeTENormalMap(S32 index, LLViewerTexture* texp);
	virtual void changeTESpecularMap(S32 index, LLViewerTexture* texp);
	LLViewerTexture* getTEImage(U8 te) const;
	LLViewerTexture* getTENormalMap(U8 te) const;
	LLViewerTexture* getTESpecularMap(U8 te) const;

	bool isImageAlphaBlended(U8 te) const;

	void fitFaceTexture(U8 face);
	// Sends packed representation of all texture entry information
	void sendTEUpdate() const;

	virtual void setScale(const LLVector3& scale, bool damped = false);

	LL_INLINE virtual F32 getStreamingCost(S32* bytes = NULL,
										   S32* visible_bytes = NULL,
										   F32* unscaled_value = NULL) const
	{
		return 0.f;
	}

	LL_INLINE virtual U32 getTriangleCount(S32* vcount = NULL) const
	{
		return 0;
	}

	LL_INLINE virtual U32 getHighLODTriangleCount()
	{
		return 0;
	}

	U32 recursiveGetTriangleCount(S32* vcount = NULL) const;

	void setObjectCost(F32 cost);
	F32 getObjectCost();

	void setLinksetCost(F32 cost);
	F32 getLinksetCost();

	void setPhysicsCost(F32 cost);
	F32 getPhysicsCost();

	void setLinksetPhysicsCost(F32 cost);
	F32 getLinksetPhysicsCost();

	F32 recursiveGetEstTrianglesMax() const;
	S32 getAnimatedObjectMaxTris() const;

	LL_INLINE virtual F32 getEstTrianglesMax() const	{ return 0.f; }

	LL_INLINE virtual F32 getEstTrianglesStreamingCost() const
	{
		return 0.f;
	}

	void sendShapeUpdate();

	LL_INLINE U8 getAttachmentState()					{ return mAttachmentState; }

	LL_INLINE F32 getAppAngle() const					{ return mAppAngle; }
	LL_INLINE F32 getPixelArea() const					{ return mPixelArea; }
	LL_INLINE void setPixelArea(F32 area)				{ mPixelArea = area; }
	F32 getMaxScale() const;
	F32 getMidScale() const;
	F32 getMinScale() const;

	// Owner id is this object's owner
	void setAttachedSound(const LLUUID& audio_uuid, const LLUUID& owner_id,
						  F32 gain, U8 flags);
	LL_INLINE void clearAttachedSound()					{ mAudioSourcep = NULL; }
	void adjustAudioGain(F32 gain);
	LL_INLINE F32 getSoundCutOffRadius() const			{ return mSoundCutOffRadius; }

	 // Creates if necessary
	LLAudioSource* getAudioSource(const LLUUID& owner_id);
	LL_INLINE bool isAudioSource() const				{ return mAudioSourcep != NULL; }

	LL_INLINE LLViewerPartSourceScript* getPartSource()	{ return mPartSourcep.get(); }

	LL_INLINE U8 getMediaType() const
	{
		return mMedia ? mMedia->mMediaType : MEDIA_NONE;
	}

	void setMediaType(U8 media_type);

	LL_INLINE const std::string& getMediaURL() const
	{
		return mMedia ? mMedia->mMediaURL : LLStringUtil::null;
	}

	void setMediaURL(const std::string& media_url);

#if 0	// Not used
	LL_INLINE bool getMediaPassedWhitelist() const
	{
		return mMedia && mMedia->mPassedWhitelist;
	}

	LL_INLINE void setMediaPassedWhitelist(bool passed)
	{
		if (mMedia)
		{
			mMedia->mPassedWhitelist = passed;
		}
	}
#endif

	void sendMaterialUpdate() const;

	void setDebugText(const std::string& utf8text);
	LLHUDIcon* setIcon(LLViewerTexture* texp, F32 scale = 0.03f);
	LL_INLINE void clearIcon()							{ mIcon = NULL; }

	void recursiveMarkForUpdate();
	virtual void markForUpdate(bool rebuild_all = false);
	void updateVolume(const LLVolumeParams& volume_params);
	virtual	void updateSpatialExtents(LLVector4a& min, LLVector4a& max);
	virtual F32 getBinRadius();
	LLBBox getBoundingBoxAgent() const;

	// Updates the global and region position caches from the object (and
	// parent's) xform.
	void updatePositionCaches() const;

	// Update text label position
	void updateText();

	// Force updates on static objects
	virtual void updateDrawable(bool force_damped);

	void setDrawableState(U32 state, bool recursive = true);
	void clearDrawableState(U32 state, bool recursive = true);
	bool isDrawableState(U32 state, bool recursive = true) const;

	// Called when the drawable shifts
	LL_INLINE virtual void onShift(const LLVector4a& shift_vector)
	{
	}

	//////////////////////////////////////
	//
	// Inventory methods
	//

	// This method is called when someone is interested in a viewer object's
	// inventory. The callback is called as soon as the viewer object has the
	// inventory stored locally.
	void registerInventoryListener(LLVOInventoryListener* listener,
								   void* user_data);
	void removeInventoryListener(LLVOInventoryListener* listener);

	enum EInventoryRequestState
	{
		INVENTORY_REQUEST_STOPPED,
		INVENTORY_REQUEST_PENDING,
		INVENTORY_XFER
	};

	LL_INLINE bool isInventoryPending()
	{
		return mInvRequestState != INVENTORY_REQUEST_STOPPED;
	}

	void clearInventoryListeners();
	LL_INLINE bool hasInventoryListeners()			{ return !mInventoryCallbacks.empty(); }
	void requestInventory();
	static void processTaskInv(LLMessageSystem* msg, void** user_data);
	void removeInventory(const LLUUID& item_id);

	// The updateInventory() call potentially calls into the selection manager,
	// so do no call updateInventory() from the selection manager until we have
	// better iterators.
	void updateInventory(LLViewerInventoryItem* itemp, bool is_new = false);

	LLInventoryObject* getInventoryObject(const LLUUID& item_id);
	LLInventoryItem* getInventoryItem(const LLUUID& item_id);
	void getInventoryContents(LLInventoryObject::object_list_t& objects);
	LLInventoryObject* getInventoryRoot();
	// Gets the object inventory item corresponding to a given asset Id, and
	// when 'type' is specified (and not AT_NONE), matching that type as well
	// (else the asset type is ignored).
	LLViewerInventoryItem* getInventoryItemByAsset(const LLUUID& asset_id,
												   LLAssetType::EType type =
														LLAssetType::AT_NONE);
	LL_INLINE S16 getInventorySerial() const		{ return mInventorySerialNum; }

	// This methods does viewer-side only object inventory modifications
	void updateViewerInventoryAsset(const LLViewerInventoryItem* item,
									const LLUUID& new_asset);

	// This method will make sure that we refresh the inventory.
	void dirtyInventory();
	LL_INLINE bool isInventoryDirty()				{ return mInventoryDirty; }

	// Saves a script, which involves removing the old one, and rezzing in the
	// new one. This method should be called with the asset Id of the new and
	// old script AFTER the bytecode has been saved.
	void saveScript(const LLViewerInventoryItem* item, bool active,
					bool is_new);

	// Moves an inventory item out of the task and into agent inventory. This
	// operation is based on messaging. No permission checks are made on the
	// viewer; the server will double-check.
	void moveInventory(const LLUUID& agent_folder, const LLUUID& item_id);

	// Finds the number of instances of this object's inventory that are of the
	// given type
	S32 countInventoryContents(LLAssetType::EType type);

	bool permAnyOwner() const;
	bool permYouOwner() const;
	bool permGroupOwner() const;
	bool permOwnerModify() const;
	bool permModify() const;
	bool permCopy() const;
	bool permMove() const;
	bool permTransfer() const;
	LL_INLINE bool flagUsePhysics() const			{ return (mFlags & FLAGS_USE_PHYSICS) != 0; }
	LL_INLINE bool flagObjectAnyOwner() const		{ return (mFlags & FLAGS_OBJECT_ANY_OWNER) != 0; }
	LL_INLINE bool flagObjectYouOwner() const		{ return (mFlags & FLAGS_OBJECT_YOU_OWNER) != 0; }
	LL_INLINE bool flagObjectGroupOwned() const		{ return (mFlags & FLAGS_OBJECT_GROUP_OWNED) != 0; }
	LL_INLINE bool flagObjectOwnerModify() const	{ return (mFlags & FLAGS_OBJECT_OWNER_MODIFY) != 0; }
	LL_INLINE bool flagObjectModify() const			{ return (mFlags & FLAGS_OBJECT_MODIFY) != 0; }
	LL_INLINE bool flagObjectCopy() const			{ return (mFlags & FLAGS_OBJECT_COPY) != 0; }
	LL_INLINE bool flagObjectMove() const			{ return (mFlags & FLAGS_OBJECT_MOVE) != 0; }
	LL_INLINE bool flagObjectTransfer() const		{ return (mFlags & FLAGS_OBJECT_TRANSFER) != 0; }
	LL_INLINE bool flagObjectPermanent() const		{ return (mFlags & FLAGS_AFFECTS_NAVMESH) != 0; }
	LL_INLINE bool flagCharacter() const			{ return (mFlags & FLAGS_CHARACTER) != 0; }
	LL_INLINE bool flagVolumeDetect() const			{ return (mFlags & FLAGS_VOLUME_DETECT) != 0; }
	LL_INLINE bool flagIncludeInSearch() const		{ return (mFlags & FLAGS_INCLUDE_IN_SEARCH) != 0; }
	LL_INLINE bool flagScripted() const				{ return (mFlags & FLAGS_SCRIPTED) != 0; }
	LL_INLINE bool flagHandleTouch() const			{ return (mFlags & FLAGS_HANDLE_TOUCH) != 0; }
	LL_INLINE bool flagTakesMoney() const			{ return (mFlags & FLAGS_TAKES_MONEY) != 0; }
	LL_INLINE bool flagPhantom() const				{ return (mFlags & FLAGS_PHANTOM) != 0; }
	LL_INLINE bool flagInventoryEmpty() const		{ return (mFlags & FLAGS_INVENTORY_EMPTY) != 0; }
	LL_INLINE bool flagAllowInventoryAdd() const	{ return (mFlags & FLAGS_ALLOW_INVENTORY_DROP) != 0; }
	LL_INLINE bool flagTemporaryOnRez() const		{ return (mFlags & FLAGS_TEMPORARY_ON_REZ) != 0; }
	LL_INLINE bool flagAnimSource() const			{ return (mFlags & FLAGS_ANIM_SOURCE) != 0; }
	LL_INLINE bool flagCameraSource() const			{ return (mFlags & FLAGS_CAMERA_SOURCE) != 0; }
	LL_INLINE bool flagCameraDecoupled() const		{ return (mFlags & FLAGS_CAMERA_DECOUPLED) != 0; }
	LL_INLINE bool flagsLoaded() const				{ return mFlagsLoaded; }

	U8 getPhysicsShapeType() const;
	LL_INLINE F32 getPhysicsGravity() const       	{ return mPhysicsGravity; }
	LL_INLINE F32 getPhysicsFriction() const      	{ return mPhysicsFriction; }
	LL_INLINE F32 getPhysicsDensity() const       	{ return mPhysicsDensity; }
	LL_INLINE F32 getPhysicsRestitution() const   	{ return mPhysicsRestitution; }

	bool isPermanentEnforced() const;

	bool isHiglightedOrBeacon() const;

	bool getIncludeInSearch() const;
	void setIncludeInSearch(bool include_in_search);

	// Does "open" object menu item apply ?
	bool allowOpen() const;

	LL_INLINE void setClickAction(U8 action)		{ mClickAction = action; }
	LL_INLINE U8 getClickAction() const				{ return mClickAction; }

	// Returns true if it got a special hover cursor
	bool specialHoverCursor() const;

	void setRegion(LLViewerRegion* regionp);

	LL_INLINE virtual void updateRegion(LLViewerRegion*)
	{
	}

	void updateFlags(bool physics_changed = false);
	void loadFlags(U32 flags);		// loads flags from cache or from message
	bool setFlags(U32 flag, bool state);
	bool setFlagsWithoutUpdate(U32 flag, bool state);
	void setPhysicsShapeType(U8 type);
	void setPhysicsGravity(F32 gravity);
	void setPhysicsFriction(F32 friction);
	void setPhysicsDensity(F32 density);
	void setPhysicsRestitution(F32 restitution);

	virtual void dump() const;

	LL_INLINE static U32 getNumObjects()			{ return sNumObjects; }

	void printNameValuePairs() const;

	LL_INLINE virtual S32 getLOD() const			{ return 3; }

	// Allows to (un)lock the LOD of all child objects of this object's root
	// object. Returns true if at least one primitive LOD got (un)locked.
	bool recursiveSetMaxLOD(bool lock = true);
	// Returns true when one (or more) of the child objects of this object's
	// root object is locked at max LOD.
	bool isLockedAtMaxLOD();

	virtual U32 getPartitionType() const;

	void dirtySpatialGroup() const;
	virtual void dirtyMesh();

	// Note: these used to be declared as virtual, but no child class was
	// overriding them... So, let's keep things simple, shall we ?
	bool setParameterEntry(U16 param_type, const LLNetworkData& new_value,
						   bool local_origin);
	bool getParameterEntryInUse(U16 param_type) const;
	bool setParameterEntryInUse(U16 param_type, bool in_use,
								bool local_origin);

	// Shortcut methods to avoid using getParameterEntry() (sometimes together
	// with getParameterEntryInUse()) and speed things up; they return a NULL
	// pointer when the extra parameter does not exist or is not in use. HB
	LLFlexibleObjectData* getFlexibleObjectData() const;
	LLLightParams* getLightParams() const;
	LLSculptParams* getSculptParams() const;
	LLLightImageParams* getLightImageParams() const;
	LLExtendedMeshParams* getExtendedMeshParams() const;
	LLRenderMaterialParams* getMaterialRenderParams() const;
	LLReflectionProbeParams* getReflectionProbeParams() const;

	// Called when a parameter is changed
	virtual void parameterChanged(U16 param_type, bool local_origin);
	virtual void parameterChanged(U16 param_type, LLNetworkData* data,
								  bool in_use, bool local_origin);

	LL_INLINE bool isShrinkWrapped() const			{ return mShouldShrinkWrap; }
	// Used to improve performance; If an object is likely to rebuild its
	// vertex buffer often as a side effect of some update (color update,
	// scale, etc), setting this to true will cause it to be pushed deeper
	// into the octree and isolate it from other nodes so that nearby objects
	// would not attempt to share a vertex buffer with this object.
	void shrinkWrap();

	static void unpackVector3(LLDataPackerBinaryBuffer* dp, LLVector3& value,
							  std::string name);
	static void unpackUUID(LLDataPackerBinaryBuffer* dp, LLUUID& value,
						   std::string name);
	static void unpackU32(LLDataPackerBinaryBuffer* dp, U32& value,
						  std::string name);
	static void unpackU8(LLDataPackerBinaryBuffer* dp, U8& value,
						 std::string name);
	static U32 unpackParentID(LLDataPackerBinaryBuffer* dp, U32& parent_id);

	// Counter-translation
	void resetChildrenPosition(const LLVector3& offset,
							   bool simplified = false,
							   bool skip_avatar_child = false);
	// Counter-rotation
	void resetChildrenRotationAndPosition(const std::vector<LLQuaternion>& rot,
										  const std::vector<LLVector3>& pos);
	void saveUnselectedChildrenRotation(std::vector<LLQuaternion>& rotations);
	void saveUnselectedChildrenPosition(std::vector<LLVector3>& positions);
	std::vector<LLVector3> mUnselectedChildrenPositions;

	LL_INLINE const LLUUID& getAttachmentItemID() const
	{
		return mAttachmentItemID;
	}

	LL_INLINE void setAttachmentItemID(const LLUUID& id)
	{
		mAttachmentItemID = id;
	}

	// Find and sets the inventory item ID of the attached object:
	const LLUUID& extractAttachmentItemID();

	LL_INLINE EObjectUpdateType getLastUpdateType() const
	{
		return mLastUpdateType;
	}

	LL_INLINE void setLastUpdateType(EObjectUpdateType type)
	{
		mLastUpdateType = type;
	}

	LL_INLINE bool getLastUpdateCached() const		{ return mLastUpdateCached; }
	LL_INLINE void setLastUpdateCached(bool b)		{ mLastUpdateCached = b; }

	virtual void updateRiggingInfo()				{}

	LL_INLINE bool getDebugUpdateMsg()				{ return mDebugUpdateMsg; }
	LL_INLINE void setDebugUpdateMsg()				{ mDebugUpdateMsg = true; }
	void toggleDebugUpdateMsg();
	static void setDebugObjectId(const LLUUID& id);

	LL_INLINE static const LLUUID& getDebuggedObjectId()
	{
		return sDebugObjectId;
	}

	LL_INLINE static bool isDebuggedObject(const LLUUID& object_id)
	{
		return object_id == sDebugObjectId;
	}

	LL_INLINE static void setVelocityInterpolate(bool value)
	{
		sVelocityInterpolate = value;
	}

	LL_INLINE static void setPingInterpolate(bool value)
	{
		sPingInterpolate = value;
	}

	static void setUpdateInterpolationTimes(F32 interpolate_time,
											F32 phase_out_time,
											F32 region_interp_time);

	// Flags for createObject()
	static constexpr S32 CO_FLAG_UI_AVATAR = 1 << 0;
	static constexpr S32 CO_FLAG_PUPPET_AVATAR = 1 << 1;

protected:
	// Delete an item in the inventory, but do not tell the server. This is
	// used internally by remove, update, and save script.
	void deleteInventoryItem(const LLUUID& item_id);

	// Do the update/caching logic. called by saveScript and updateInventory.
	void doUpdateInventory(LLPointer<LLViewerInventoryItem>& itemp,
						   bool is_new);

	static LLViewerObject* createObject(const LLUUID& id, LLPCode pcode,
										LLViewerRegion* regionp,
										S32 flags = 0);

	bool setData(const U8* datap, U32 data_size);

	// Hide or show HUD, icon and particles
	void hideExtraDisplayItems(bool hidden);

	LL_INLINE bool isOnMap()						{ return mOnMap; }

	//////////////////////////
	// Inventory functionality

	static void processTaskInvFile(void** user_data, S32 error_code,
								   LLExtStat ext_status);
	bool loadTaskInvFile(const std::string& filename);
	void doInventoryCallback();

	void unpackParticleSource(S32 block_num, const LLUUID& owner_id);
	void unpackParticleSource(LLDataPacker& dp, const LLUUID& owner_id,
							  bool legacy);
	void deleteParticleSource();
	void setParticleSource(const LLPartSysData& particle_params,
						   const LLUUID& owner_id);

private:
	bool isAssetInInventory(LLViewerInventoryItem* itemp,
							LLAssetType::EType type);

	LLNetworkData* createNewParameterEntry(U16 param_type);
	LLNetworkData* getExtraParameterEntry(U16 param_type) const;
	LLNetworkData* getExtraParameterEntryCreate(U16 param_type);
	bool unpackParameterEntry(U16 param_type, LLDataPacker* dp);

	// This method checks to see if the given media URL has changed its
	// version and the update wasn't due to this agent's last action.
	U32 checkMediaURL(const std::string& media_url);

	// Motion prediction between updates
	void interpolateLinearMotion(F64 time, F32 dt);

	void applyAngularVelocity(F32 dt);

	// Clears NV pairs and then individually adds \n separated NV pairs from
	// \0 terminated string
	void setNameValueList(const std::string& list);

	void deleteTEImages(); // correctly deletes list of images

	static void initObjectDataMap();

	void fetchInventoryFromServer();

	void setRenderMaterialIDs(const LLRenderMaterialParams* paramsp,
							  bool local_origin);
	void rebuildMaterial();

	LLViewerFetchedTexture* getFetchedTexForMat(const LLUUID& id, F32 vsize,
												U32 prio = 0); // BOOST_NONE

public:
	LLUUID								mID;
	LLUUID								mOwnerID;	// null if unknown

	LLPointer<LLVOAvatarPuppet>			mPuppetAvatar;

	LLJointRiggingInfoTab				mJointRiggingInfoTab;

	// Pipeline classes
	LLPointer<LLDrawable>				mDrawable;

	// *TODO: make all this stuff private.  JC
	LLPointer<LLHUDIcon>				mIcon;
	LLPointer<LLHUDText>				mText;

	LLPointer<LLReflectionMap>			mReflectionProbe;

	LLColor4							mHudTextColor;

	// Cached values used to restore the text after switching off debug info
	// hovertexts:
	std::string							mHudTextString;

	// Unique within region, not unique across regions. Local ID=0 is not used
	U32									mLocalID;

	// Last total CRC received from sim, used for caching
	U32									mTotalCRC;

	// Index into LLViewerObjectList::mActiveObjects or -1 if not in list
	S32									mListIndex;

	// In bits
	S32									mBestUpdatePrecision;

	// Sent to sim in UPDATE_FLAGS, received in ObjectPhysicsProperties
	U8									mPhysicsShapeType;
	F32									mPhysicsGravity;
	F32									mPhysicsFriction;
	F32									mPhysicsDensity;
	F32									mPhysicsRestitution;

	// True if user can select this object by clicking
	bool								mCanSelect;

	// Band-aid to select object after all creation initialization is done
	bool								mCreateSelected;

	bool								mIsReflectionProbe;

	static bool							sUseNewTargetOmegaCode;

protected:
	// Region that this object belongs to.
	LLViewerRegion*						mRegionp;

	// Last update for purposes of interpolation
	F64									mLastInterpUpdateSecs;
	// Region crossing interpolation expiry time
	F64									mRegionCrossExpire;
	// Last update from a message from the simulator
	F64									mLastMessageUpdateSecs;
	// Latest time stamp on message from simulator
	TPACKETID							mLatestRecvPacketID;
	// Extra data sent from the sim...currently only used for tree species info
	U8*									mData;

	// Particle source associated with this object.
	LLPointer<LLViewerPartSourceScript> mPartSourcep;
	LLAudioSourceVO*					mAudioSourcep;
	F32									mAudioGain;
	F32									mSoundCutOffRadius;

	// Apparent visual arc in degrees
	F32									mAppAngle;
	// Apparent area in pixels
	F32									mPixelArea;

	EInventoryRequestState				mInvRequestState;
	U64									mInvRequestXFerId;

	S32									mNumFaces;

	// Amount (in seconds) that object has rotated according to angular
	// velocity (llSetTargetOmega)
	F32									mRotTime;

	// Accumulated rotation from the angular velocity computations
	LLQuaternion						mAngularVelocityRot;
	LLQuaternion						mPreviousRotation;

	// NULL if no media associated
	LLViewerObjectMedia*				mMedia;

	// Resource cost of this object or -1 if unknown
	F32									mObjectCost;
	F32									mLinksetCost;
	F32									mPhysicsCost;
	F32									mLinksetPhysicsCost;

	// These two caches are only correct for non-parented objects right now !
	mutable LLVector3					mPositionRegion;
	mutable LLVector3					mPositionAgent;

	// This is the object's inventory from the viewer's perspective.
	S16									mInventorySerialNum;
	S16									mExpectedInventorySerialNum;
	LLInventoryObject::object_list_t*	mInventory;
	callback_list_t						mInventoryCallbacks;
	// Ids of itemsadded to the contents of the object but have not yet been
	// updated on the server.
	uuid_list_t							mPendingInventoryItemsIDs;

	// Any name-value pairs stored by script
	typedef std::map<char*, LLNameValue*> name_value_map_t;
	name_value_map_t 					mNameValuePairs;

	child_list_t						mChildList;

	// There used to be an 'mExtraParameterList' parameter-type-keyed map of
	// pointers to heap-allocated instances of an ExtraParameter structure,
	// itself conntaining a LLNetworkData* 'data' pointer and an 'in_use' bool.
	// Given the small number (9, as I am writing this) of extra parameters, it
	// is way more efficient to use the two tiny arrays below, that get
	// allocated together with the rest of the member variables (no additionnal
	// heap allocation needed and cache locality ensured). Plus, with the use
	// of a seperate mExtraParameterInUse array (which entries are all cleared
	// to false and can be true only after the corresponding LLNetworkData
	// structure has been successfully allocated), we save ourselves a check
	// on the LLNetworkData pointers validity (non-NULL value)... HB
	LLNetworkData*						mExtraParameters[LL_EPARAMS_COUNT];
	bool								mExtraParameterInUse[LL_EPARAMS_COUNT];

	U8									mClickAction;
	// This encodes the attachment id in a somewhat complex way. 0 if not an
	// attachment.
	U8									mAttachmentState;

	bool								mShouldShrinkWrap;

	mutable bool						mPhysicsShapeUnknown;

	bool								mCostStale;

	bool								mInventoryDirty;
	bool								mDead;
	// true when this is an orphaned child:
	bool								mOrphaned;
	// Cached user select information:
	bool								mUserSelected;
	bool								mOnActiveList;
	bool								mOnMap;		// On the map.
	bool								mStatic;	// Object does not move

private:
	bool								mFlagsLoaded;
	bool								mLastUpdateCached;

	bool								mDebugUpdateMsg;

	typedef std::vector<LLPointer<LLViewerTexture> > te_images_vec_t;
	te_images_vec_t						mTEImages;
	te_images_vec_t						mTENormalMaps;
	te_images_vec_t						mTESpecularMaps;

	// ItemID of the associated object is in user inventory.
	LLUUID								mAttachmentItemID;

	// Grabbed from UPDATE_FLAGS
	U32									mFlags;
	EObjectUpdateType					mLastUpdateType;

	static LLUUID						sDebugObjectId;

	static S32							sNumObjects;

	// For motion interpolation
	static F64							sPhaseOutUpdateInterpolationTime;
	static F64							sMaxUpdateInterpolationTime;
	static F64							sMaxRegionCrossingInterpolationTime;

	static bool							sVelocityInterpolate;
	static bool							sPingInterpolate;

	static std::map<std::string, U32>	sObjectDataMap;
};

// Sub-class of viewer object that can be added to particle partitions
class LLAlphaObject : public LLViewerObject
{
public:
	LLAlphaObject(const LLUUID& id, LLPCode pcode, LLViewerRegion* regionp)
	:	LLViewerObject(id, pcode, regionp),
		mDepth(0.f)
	{
	}

	LL_INLINE LLAlphaObject* asAlphaObject() override
	{
		return this;
	}

	LL_INLINE virtual F32 getPartSize(S32 idx)		{ return 0.f; }

	virtual void getGeometry(S32 idx,
							 LLStrider<LLVector4a>& verticesp,
							 LLStrider<LLVector3>& normalsp,
							 LLStrider<LLVector2>& texcoordsp,
							 LLStrider<LLColor4U>& colorsp,
							 LLStrider<LLColor4U>& emissivep,
							 LLStrider<U16>& indicesp) = 0;

	LL_INLINE virtual bool getBlendFunc(S32 face, U32& src, U32& dst)
	{
		return true;
	}

public:
	F32 mDepth;
};

class LLStaticViewerObject : public LLViewerObject
{
public:
	LLStaticViewerObject(const LLUUID& id, LLPCode pcode,
						 LLViewerRegion* regionp, bool is_global = false)
	:	LLViewerObject(id, pcode, regionp, is_global)
	{
	}

	void updateDrawable(bool force_damped) override;
};

#endif
