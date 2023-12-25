/**
 * @file llselectmgr.h
 * @brief A manager for selected objects and TEs.
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

#ifndef LL_LLSELECTMGR_H
#define LL_LLSELECTMGR_H

#include "boost/iterator/filter_iterator.hpp"
#include "boost/signals2.hpp"

#include "llbbox.h"
#include "llcategory.h"
#include "llcharacter.h"
#include "llcontrol.h"
#include "llcoord.h"
#include "lleditmenuhandler.h"
#include "llframetimer.h"
#include "llgltfmaterial.h"				// For gltf_mat_vec_t
#include "llmaterial.h"
#include "llpermissions.h"
#include "llpointer.h"
#include "llquaternion.h"
#include "llrender.h"
#include "llsafehandle.h"
#include "llsaleinfo.h"
#include "llundo.h"
#include "lluuid.h"
#include "llvector3d.h"

#include "llviewerobject.h"

class LLDrawable;
class LLSelectNode;
class LLViewerInventoryItem;
class LLViewerTexture;
class LLVOVolume;

constexpr U8 UPD_NONE      		= 0x00;
constexpr U8 UPD_POSITION  		= 0x01;
constexpr U8 UPD_ROTATION  		= 0x02;
constexpr U8 UPD_SCALE     		= 0x04;
constexpr U8 UPD_LINKED_SETS 	= 0x08;
constexpr U8 UPD_UNIFORM 		= 0x10;	// used with UPD_SCALE

// This is used by the DeRezObject message to determine where to put
// derezed tasks.
enum EDeRezDestination
{
	DRD_SAVE_INTO_AGENT_INVENTORY = 0,
	DRD_ACQUIRE_TO_AGENT_INVENTORY = 1,		// try to leave copy in world
	DRD_SAVE_INTO_TASK_INVENTORY = 2,
	DRD_ATTACHMENT = 3,
	DRD_TAKE_INTO_AGENT_INVENTORY = 4,		// delete from world
	DRD_FORCE_TO_GOD_INVENTORY = 5,			// force take copy
	DRD_TRASH = 6,
	DRD_ATTACHMENT_TO_INV = 7,
	DRD_ATTACHMENT_EXISTS = 8,
	DRD_RETURN_TO_OWNER = 9,				// back to owner's inventory
	DRD_RETURN_TO_LAST_OWNER = 10,			// deeded object back to last owner's inventory

	DRD_COUNT = 11
};

constexpr S32 SELECT_ALL_TES = -1;
constexpr S32 SELECT_MAX_TES = 32;

// Do something to all objects in the selection manager.
// The bool return value can be used to indicate if all
// objects are identical (gathering information) or if
// the operation was successful.
struct LLSelectedObjectFunctor
{
	virtual ~LLSelectedObjectFunctor() = default;
	virtual bool apply(LLViewerObject* object) = 0;
};

// Do something to all select nodes in the selection manager.
// The bool return value can be used to indicate if all
// objects are identical (gathering information) or if
// the operation was successful.
struct LLSelectedNodeFunctor
{
	virtual ~LLSelectedNodeFunctor() = default;
	virtual bool apply(LLSelectNode* node) = 0;
};

struct LLSelectedTEFunctor
{
	virtual ~LLSelectedTEFunctor() = default;
	virtual bool apply(LLViewerObject* object, S32 face) = 0;
};

struct LLSelectedTEMaterialFunctor
{
	virtual ~LLSelectedTEMaterialFunctor() = default;
	virtual LLMaterialPtr apply(LLViewerObject* object, S32 face,
								LLTextureEntry* tep,
								LLMaterialPtr& current_material) = 0;
};

template <typename T> struct LLSelectedTEGetFunctor
{
	virtual ~LLSelectedTEGetFunctor() = default;
	virtual T get(LLViewerObject* object, S32 te) = 0;
};

typedef enum e_send_type
{
	SEND_ONLY_ROOTS,
	SEND_INDIVIDUALS,
	SEND_ROOTS_FIRST, // useful for serial undos on linked sets
	SEND_CHILDREN_FIRST // useful for serial transforms of linked sets
} ESendType;

typedef enum e_grid_mode
{
	GRID_MODE_WORLD,
	GRID_MODE_LOCAL,
	GRID_MODE_REF_OBJECT
} EGridMode;

typedef enum e_action_type
{
	SELECT_ACTION_TYPE_BEGIN,
	SELECT_ACTION_TYPE_PICK,
	SELECT_ACTION_TYPE_MOVE,
	SELECT_ACTION_TYPE_ROTATE,
	SELECT_ACTION_TYPE_SCALE,
	NUM_ACTION_TYPES
} EActionType;

typedef enum e_selection_type
{
	SELECT_TYPE_WORLD,
	SELECT_TYPE_ATTACHMENT,
	SELECT_TYPE_HUD
} ESelectType;

constexpr S32 TE_SELECT_MASK_ALL = 0xFFFFFFFF;

// Contains information about a selected object, particularly which TEs are selected.
class LLSelectNode
{
protected:
	LOG_CLASS(LLSelectNode);

public:
	LLSelectNode(LLViewerObject* object, bool do_glow);
	LLSelectNode(const LLSelectNode& nodep);
	~LLSelectNode();

	void selectAllTEs(bool b);
	void selectTE(S32 te_index, bool selected);
	bool isTESelected(S32 te_index);
	S32 getLastSelectedTE();
	LL_INLINE S32 getLastOperatedTE()				{ return mLastTESelected; }
	S32 getTESelectMask()							{ return mTESelectMask; }

	void renderOneWireframe(const LLColor4& color);
	void renderOneSilhouette(const LLColor4& color, bool no_hidden = false);

	LL_INLINE void setTransient(bool transient)		{ mTransient = transient; }
	LL_INLINE bool isTransient()					{ return mTransient; }

	LLViewerObject* getObject();
	LL_INLINE void setObject(LLViewerObject* obj)	{ mObject = obj; }

	// *NOTE: invalidate stored textures and colors when # faces change
	void saveColors();
	void saveTextures(const uuid_vec_t& tex_ids);
	void saveTextureScaleRatios();

	void saveGLTFMaterials(const uuid_vec_t& mat_ids,
						   const gltf_mat_vec_t& override_mats);

	bool allowOperationOnNode(PermissionBit op, U64 group_proxy_power) const;

public:
	LLPermissions*	mPermissions;
	LLSaleInfo		mSaleInfo;
	LLAggregatePermissions mAggregatePerm;
	LLAggregatePermissions mAggregateTexturePerm;
	LLAggregatePermissions mAggregateTexturePermOwner;
	LLCategory		mCategory;
	S16				mInventorySerial;
	// For interactively modifying object position
	LLVector3		mSavedPositionLocal;
	LLVector3		mLastPositionLocal;
	// For interactively modifying object position
	LLVector3d		mSavedPositionGlobal;
	// For interactively modifying object scale
	LLVector3		mSavedScale;
	LLVector3		mLastScale;
	// For interactively modifying object rotation
	LLQuaternion	mSavedRotation;
	LLQuaternion	mLastRotation;
	LLVector3d		mDuplicatePos;
	LLQuaternion	mDuplicateRot;
	LLUUID			mItemID;
	LLUUID			mFolderID;
	LLUUID			mFromTaskID;
	U64				mCreationDate;
	// For root objects and objects individually selected
	bool			mIndividualSelection;

	bool			mTransient;
	bool			mValid;					// Is extra information valid ?

	bool			mDuplicated;

	bool			mSilhouetteGenerated;	// Need to generate silhouette ?

	std::string		mName;
	std::string		mDescription;
	std::string		mTouchName;
	std::string		mSitName;

	// array of vertices to render silhouette of object:
	std::vector<LLVector3>	mSilhouetteVertices;
	// array of normals to render silhouette of object:
	std::vector<LLVector3>	mSilhouetteNormals;
	std::vector<LLColor4>	mSavedColors;
	uuid_vec_t				mSavedTextures;
	uuid_vec_t				mSavedGLTFMaterialIds;
	gltf_mat_vec_t			mSavedGLTFOverrideMaterials;
	std::vector<LLVector3>  mTextureScaleRatios;

protected:
	LLPointer<LLViewerObject>	mObject;
	S32				mTESelectMask;
	S32				mLastTESelected;
};

class LLObjectSelection : public LLRefCount
{
	friend class LLSelectMgr;
	friend class LLSafeHandle<LLObjectSelection>;

protected:
	LOG_CLASS(LLObjectSelection);

	~LLObjectSelection();

public:
	typedef std::list<LLSelectNode*> list_t;

public:
	// Iterators
	struct is_non_null
	{
		bool operator()(LLSelectNode* node)
		{
			return node->getObject() != NULL;
		}
	};
	typedef boost::filter_iterator<is_non_null, list_t::iterator > iterator;
	LL_INLINE iterator begin()						{ return iterator(mList.begin(), mList.end()); }
	LL_INLINE iterator end()						{ return iterator(mList.end(), mList.end()); }

	struct is_valid
	{
		bool operator()(LLSelectNode* node)
		{
			return node->getObject() != NULL && node->mValid;
		}
	};
	typedef boost::filter_iterator<is_valid, list_t::iterator > valid_iterator;
	LL_INLINE valid_iterator valid_begin()			{ return valid_iterator(mList.begin(), mList.end()); }
	LL_INLINE valid_iterator valid_end()			{ return valid_iterator(mList.end(), mList.end()); }

	struct is_root
	{
		bool operator()(LLSelectNode* node);
	};
	typedef boost::filter_iterator<is_root, list_t::iterator > root_iterator;
	LL_INLINE root_iterator root_begin()			{ return root_iterator(mList.begin(), mList.end()); }
	LL_INLINE root_iterator root_end()				{ return root_iterator(mList.end(), mList.end()); }

	struct is_valid_root
	{
		bool operator()(LLSelectNode* node);
	};
	typedef boost::filter_iterator<is_root, list_t::iterator > valid_root_iterator;
	LL_INLINE valid_root_iterator valid_root_begin()
	{
		return valid_root_iterator(mList.begin(), mList.end());
	}

	LL_INLINE valid_root_iterator valid_root_end()	{ return valid_root_iterator(mList.end(), mList.end()); }

	struct is_root_object
	{
		bool operator()(LLSelectNode* node);
	};
	typedef boost::filter_iterator<is_root_object, list_t::iterator > root_object_iterator;
	LL_INLINE root_object_iterator root_object_begin()
	{
		return root_object_iterator(mList.begin(), mList.end());
	}

	LL_INLINE root_object_iterator root_object_end()
	{
		return root_object_iterator(mList.end(), mList.end());
	}

public:
	LLObjectSelection();

	LL_INLINE void updateEffects()						{}

	LL_INLINE bool isEmpty() const						{ return mList.size() == 0; }

	LLSelectNode* getFirstNode(LLSelectedNodeFunctor* func = NULL);
	LLSelectNode* getFirstRootNode(LLSelectedNodeFunctor* func = NULL,
								   bool non_root_ok = false);
	LLSelectNode* getFirstMoveableNode(bool get_root_first = false);

	LL_INLINE LLViewerObject* getFirstObject()
	{
		LLSelectNode* res = getFirstNode(NULL);
		return res ? res->getObject() : NULL;
	}

	LL_INLINE LLViewerObject* getFirstRootObject(bool non_root_ok = false)
	{
		LLSelectNode* res = getFirstRootNode(NULL, non_root_ok);
		return res ? res->getObject() : NULL;
	}

	LLViewerObject* getFirstSelectedObject(LLSelectedNodeFunctor* func,
										   bool get_parent = false);
	LLViewerObject*	getFirstEditableObject(bool get_parent = false);
	LLViewerObject*	getFirstCopyableObject(bool get_parent = false);
	LLViewerObject* getFirstDeleteableObject();
	LLViewerObject*	getFirstMoveableObject(bool get_parent = false);
	LLViewerObject*	getFirstUndoEnabledObject(bool get_parent = false);

	// Return the object that lead to this selection, possibly a child
	LL_INLINE LLViewerObject* getPrimaryObject()	{ return mPrimaryObject; }

	// Methods to iterate through texture entries
	bool getSelectedTEValue(LLSelectedTEGetFunctor<F32>* func, F32& res,
							F32 tolerance);
	template <typename T> bool getSelectedTEValue(LLSelectedTEGetFunctor<T>* func,
												  T& res);
	template <typename T> bool isMultipleTEValue(LLSelectedTEGetFunctor<T>* func,
												 const T& ignore_value);

	S32 getNumNodes();
	LLSelectNode* findNode(LLViewerObject* objectp);

	// Count members
	LL_INLINE S32 getObjectCount()					{ cleanupNodes(); return (S32)mList.size(); }
	F32 getSelectedObjectCost();
	F32 getSelectedLinksetCost();
	F32 getSelectedPhysicsCost();
	F32 getSelectedLinksetPhysicsCost();

	F32 getSelectedObjectStreamingCost(S32* total_bytes = NULL,
									   S32* visible_bytes = NULL);
	U32 getSelectedObjectTriangleCount(S32* vcount = NULL);

	S32 getTECount();
	S32 getRootObjectCount();

	bool isMultipleTESelected();
	bool contains(LLViewerObject* object);
	bool contains(LLViewerObject* object, S32 te);

	// Returns true is any node is currenly worn as an attachment
	bool isAttachment();

	// AXON: validate a potential link against limits
	bool checkAnimatedObjectEstTris();

	// Apply functors to various subsets of the selected objects. If firstonly
	// is false, returns the AND of all apply() calls. Else returns true
	// immediately if any apply() call succeeds (i.e. OR with early exit)
	bool applyToRootObjects(LLSelectedObjectFunctor* func,
							bool firstonly = false);
	bool applyToObjects(LLSelectedObjectFunctor* func);
	bool applyToTEs(LLSelectedTEFunctor* func, bool firstonly = false);
	bool applyToRootNodes(LLSelectedNodeFunctor* func, bool firstonly = false);
	bool applyToNodes(LLSelectedNodeFunctor* func, bool firstonly = false);

	LL_INLINE ESelectType getSelectType() const		{ return mSelectType; }

private:
	void applyNoCopyTextureToTEs(LLViewerInventoryItem* itemp);
	// Multi-purpose method for applying PBR materials to the selected object
	// or faces, any combination of copy/mod/transfer permission restrictions.
	// This method moves the restricted material to the object's inventory and
	// does not make a copy of the material for each face. Then this only
	// material is used for all selected faces. Returns false if applying the
	// material failed on one or more selected faces.
	bool applyRestrictedPbrMatToTEs(LLViewerInventoryItem* itemp);

	void addNode(LLSelectNode* nodep);
	void addNodeAtEnd(LLSelectNode* nodep);
	void moveNodeToFront(LLSelectNode* nodep);
	void removeNode(LLSelectNode* nodep);
	void deleteAllNodes();
	void cleanupNodes();

private:
	list_t mList;
	const LLObjectSelection& operator=(const LLObjectSelection&);

	LLPointer<LLViewerObject> mPrimaryObject;
	std::map<LLPointer<LLViewerObject>, LLSelectNode*> mSelectNodeMap;
	ESelectType mSelectType;
};

typedef LLSafeHandle<LLObjectSelection> LLObjectSelectionHandle;

// For use with getFirstTest()
struct LLSelectGetFirstTest;

class LLSelectMgr : public LLEditMenuHandler
{
protected:
	LOG_CLASS(LLSelectMgr);

public:
	LLSelectMgr();
	~LLSelectMgr();

	void initClass();	// Called from LLAppViewer::init()

	// LLEditMenuHandler interface
	virtual bool canUndo() const;
	virtual void undo();

	virtual bool canRedo() const;
	virtual void redo();

	virtual bool canDoDelete() const;
	virtual void doDelete();

	virtual void deselect();
	virtual bool canDeselect() const;

	virtual void duplicate();
	virtual bool canDuplicate() const;

	void clearSelections();
	void update();
	void updateEffects(); // Update HUD effects
	void overrideObjectUpdates();

	// Returns the previous value of mForceSelection
	bool setForceSelection(bool force);

	////////////////////////////////////////////////////////////////
	// Selection methods
	////////////////////////////////////////////////////////////////

	static bool renderHiddenSelection();

	////////////////////////////////////////////////////////////////
	// Add
	////////////////////////////////////////////////////////////////

	// This method is meant to select an object, and then select all
	// of the ancestors and descendents. This should be the normal behavior.
	//
	// *NOTE: You must hold on to the object selection handle, otherwise
	// the objects will be automatically deselected in 1 frame.
	LLObjectSelectionHandle selectObjectAndFamily(LLViewerObject* object,
												  bool add_to_end = false);

	// For when you want just a child object.
	LLObjectSelectionHandle selectObjectOnly(LLViewerObject* object,
											 S32 face = SELECT_ALL_TES);

	// Same as above, but takes a list of objects.  Used by rectangle select.
	LLObjectSelectionHandle selectObjectAndFamily(const std::vector<LLViewerObject*>& object_list,
												  bool send_to_sim = true);

	// converts all objects currently highlighted to a selection, and returns it
	LLObjectSelectionHandle selectHighlightedObjects();

	LLObjectSelectionHandle setHoverObject(LLViewerObject* objectp,
										   S32 face = -1);

	void highlightObjectOnly(LLViewerObject* objectp);
	void highlightObjectAndFamily(LLViewerObject* objectp);
	void highlightObjectAndFamily(const std::vector<LLViewerObject*>& list);

	////////////////////////////////////////////////////////////////
	// Remove
	////////////////////////////////////////////////////////////////

	void deselectObjectOnly(LLViewerObject* object, bool send_to_sim = true);
	void deselectObjectAndFamily(LLViewerObject* object,
								 bool send_to_sim = true,
								 bool include_entire_object = false);

	// Send deselect messages to simulator, then clear the list
	void deselectAll();
	void deselectAllForStandingUp();

	// deselect only if nothing else currently referencing the selection
	void deselectUnused();

	// Deselect if the selection center is too far away from the agent.
	void deselectAllIfTooFar();

	// Removes all highlighted objects from current selection
	void deselectHighlightedObjects();

	void unhighlightObjectOnly(LLViewerObject* objectp);
	void unhighlightObjectAndFamily(LLViewerObject* objectp);
	void unhighlightAll();

	bool removeObjectFromSelections(const LLUUID& id);

	////////////////////////////////////////////////////////////////
	// Selection editing
	////////////////////////////////////////////////////////////////
	bool linkObjects();
	bool unlinkObjects();
	bool enableLinkObjects();
	bool enableUnlinkObjects();

	////////////////////////////////////////////////////////////////
	// Selection accessors
	////////////////////////////////////////////////////////////////
	LL_INLINE LLObjectSelectionHandle getSelection()
	{
		return mSelectedObjects;
	}

	// Right now this just renders the selection with root/child colors instead
	// of a single color
	LL_INLINE LLObjectSelectionHandle getEditSelection()
	{
		convertTransient();
		return mSelectedObjects;
	}

	LL_INLINE LLObjectSelectionHandle getHighlightedObjects()
	{
		return mHighlightedObjects;
	}

	LLSelectNode* getHoverNode();
	LLSelectNode* getPrimaryHoverNode();

	////////////////////////////////////////////////////////////////
	// Grid manipulation
	////////////////////////////////////////////////////////////////
	void addGridObject(LLViewerObject* objectp);
	void clearGridObjects();

	void setGridMode(EGridMode mode);
	LL_INLINE EGridMode getGridMode()				{ return mGridMode; }

	void getGrid(LLVector3& origin, LLQuaternion& rotation, LLVector3& scale,
				 bool for_snap_guides = false);

	LL_INLINE bool getTEMode()						{ return mTEMode; }
	LL_INLINE void setTEMode(bool b)				{ mTEMode = b; }

	LL_INLINE bool shouldShowSelection()			{ return mShowSelection; }

	LLBBox getBBoxOfSelection() const;

	LL_INLINE LLBBox getSavedBBoxOfSelection() const
	{
		return mSavedSelectionBBox;
	}

	void dump();
	void cleanup();

	void updateSilhouettes();
	void renderSilhouettes(bool for_hud);
	LL_INLINE void enableSilhouette(bool enable)	{ mRenderSilhouettes = enable; }

	////////////////////////////////////////////////////////////////
	// Utility functions that operate on the current selection
	////////////////////////////////////////////////////////////////
	void saveSelectedObjectTransform(EActionType action_type);
	void saveSelectedObjectColors();
	void saveSelectedObjectTextures();

	// Sets which texture channel to query for scale and rot of display and
	// depends on UI state of LLPanelFace when editing
	LL_INLINE void setTextureChannel(LLRender::eTexIndex index)
	{
		mTextureChannel = index;
	}

	LL_INLINE LLRender::eTexIndex getTextureChannel()
	{
		return mTextureChannel;
	}

	void selectionUpdatePhysics(bool use_physics);
	void selectionUpdateTemporary(bool is_temporary);
	void selectionUpdatePhantom(bool is_ghost);
	void selectionDump();

	bool selectionAllPCode(LLPCode code);		// All objects have this PCode
	bool selectionGetClickAction(U8* out_action);
	// true if all selected objects have same:
	bool selectionGetIncludeInSearch(bool* include_in_search_out);
	bool selectionGetGlow(F32* glow);

	void selectionSetPhysicsType(U8 type);
	void selectionSetGravity(F32 gravity);
	void selectionSetFriction(F32 friction);
	void selectionSetDensity(F32 density);
	void selectionSetRestitution(F32 restitution);
	void selectionSetMaterialParams(LLSelectedTEMaterialFunctor* material_func,
									S32 te = -1);
	void selectionSetMaterial(U8 material);
	void selectionSetTexture(const LLUUID& tex_id);			// Item or asset id
	bool selectionSetGLTFMaterial(const LLUUID& mat_id);	// Item or asset id
	void selectionSetColor(const LLColor4& color);
	void selectionSetColorOnly(const LLColor4& color); // Set only the RGB channels
	void selectionSetAlphaOnly(F32 alpha); // Set only the alpha channel
	void selectionRevertColors();
	bool selectionRevertTextures();
	void selectionRevertGLTFMaterials();
	void selectionSetTexGen(U8 texgen);
	void selectionSetBumpmap(U8 bumpmap);
	void selectionSetShiny(U8 shiny);
	void selectionSetFullbright(U8 fullbright);
	void selectionSetMedia(U8 media_type, const LLSD& media_data);
	void selectionSetClickAction(U8 action);
	void selectionSetIncludeInSearch(bool include_in_search);
	void selectionSetGlow(F32 glow);
#if 1
	void selectionSetMaterials(LLMaterialPtr material);
#endif
	void selectionRemoveMaterial();

	void selectionSetObjectPermissions(U8 perm_field, bool set, U32 perm_mask,
									   bool do_override = false);
	void selectionSetObjectName(const std::string& name);
	void selectionSetObjectDescription(const std::string& desc);
	void selectionSetObjectCategory(const LLCategory& category);
	void selectionSetObjectSaleInfo(const LLSaleInfo& sale_info);

	void selectionTexScaleAutofit(F32 repeats_per_meter);
	void adjustTexturesByScale(bool send_to_sim, bool stretch);

	bool selectionMove(const LLVector3& displ, F32 rx, F32 ry, F32 rz,
					   U32 update_type);
	void sendSelectionMove();

	void sendGodlikeRequest(const std::string& request,
							const std::string& parameter);

	// Will make sure all selected object meet current criteria, or deselect
	// them otherwise
	void validateSelection();

	// Returns true if it is possible to select this object
	bool canSelectObject(LLViewerObject* object);

	// Returns true when the current selection (if any) is of the avatar (not
	// HUD) attachment type. HB
	bool selectionIsAvatarAttachment();

	// Returns true if the viewer has information on all selected objects
	bool selectGetAllRootsValid();
	bool selectGetAllValid();

	// Returns true if you can modify all selected objects.
	bool selectGetRootsModify();
	bool selectGetModify();

	// Returns true if is all objects are non-permanent-enforced
	bool selectGetRootsNonPermanentEnforced();
	bool selectGetNonPermanentEnforced();

	// Returns true if is all objects are permanent
	bool selectGetRootsPermanent();
	bool selectGetPermanent();

	// Return true if is all objects are characters
	bool selectGetRootsCharacter();
	bool selectGetCharacter();

	// Returns true if is all objects are not pathfinding
	bool selectGetRootsNonPathfinding();
	bool selectGetNonPathfinding();

	// Returns true if is all objects are not permanent
	bool selectGetRootsNonPermanent();
	bool selectGetNonPermanent();

	// Returns true if is all objects are not character
	bool selectGetRootsNonCharacter();
	bool selectGetNonCharacter();

	bool selectGetEditableLinksets();
	bool selectGetViewableCharacters();

	std::string getPathFindingAttributeInfo(bool empty_for_none = false);

	// Returns true if selected objects can be transferred.
	bool selectGetRootsTransfer();

	// Returns true if selected objects can be copied.
	bool selectGetRootsCopy();

	// Returns true if all have same creator, returns id
	bool selectGetCreator(LLUUID& id, std::string& name);

	// Returns true if all objects have same owner, returns id
	bool selectGetOwner(LLUUID& id, std::string& name);

	// Returns true if all objects have same owner, returns id
	bool selectGetLastOwner(LLUUID& id, std::string& name);

	// Returns true if all are the same. id is stuffed with the value found if
	// available.
	bool selectGetGroup(LLUUID& id);

	// Returns true if all have data, returns two masks, each indicating which
	// bits are all on and all off
	bool selectGetPerm(U8 which_perm, U32* mask_on, U32* mask_off);

	// Returns true if all root objects have valid data and are group owned.
	bool selectIsGroupOwned();

	// Returns true if all the nodes are valid. Accumulates permissions in the
	// parameter.
	bool selectGetPermissions(LLPermissions& perm);

	// Returns true if all the nodes are valid. Depends onto "edit linked"
	// state. Children in linksets are a bit special: they require not only
	// move permission but also modify if "edit linked" is set, since you move
	// them relative to their parent.
	bool selectGetEditMoveLinksetPermissions(bool& move, bool& modify);

	// Get a bunch of useful sale information for the object(s) selected.
	// "_mixed" is true if not all objects have the same setting.
	void selectGetAggregateSaleInfo(U32& num_for_sale,
									bool& is_for_sale_mixed,
									bool& is_sale_price_mixed,
									S32& total_sale_price,
									S32& individual_sale_price);

	// Returns true if all nodes are valid. Method also stores an accumulated
	// sale info.
	bool selectGetSaleInfo(LLSaleInfo& sale_info);

	// Returns true if all nodes are valid. fills passed in object
	// with the aggregate permissions of the selection.
	bool selectGetAggregatePermissions(LLAggregatePermissions& ag_perm);

	// Returns true if all nodes are valid. fills passed in object with the
	// aggregate permissions for texture inventory items of the selection.
	bool selectGetAggregateTexturePermissions(LLAggregatePermissions& ag_perm);

	LLPermissions* findObjectPermissions(const LLViewerObject* object);

	void selectDelete();				// Delete on simulator
	void selectForceDelete();			// just delete, no into trash
	// Duplicate on simulator:
	void selectDuplicate(const LLVector3& offset, bool select_copy);
	void repeatDuplicate();
	void selectDuplicateOnRay(const LLVector3& ray_start_region,
							  const LLVector3& ray_end_region,
							  bool bypass_raycast,
							  bool ray_end_is_intersection,
							  const LLUUID& ray_target_id,
							  bool copy_centers,
							  bool copy_rotates,
							  bool select_copy);

	void sendMultipleUpdate(U32 type);	// Position, rotation, scale all in one
	void sendOwner(const LLUUID& owner_id, const LLUUID& group_id,
				   bool do_override = false);
	void sendGroup(const LLUUID& group_id);

	// Category ID is the UUID of the folder you want to contain the purchase.
	// *NOTE: sale_info check doesn't work for multiple object buy, which UI
	// does not currently support sale info is used for verification only, if
	// it does not match region info then the sale is cancelled.
	void sendBuy(const LLUUID& buyer_id, const LLUUID& category_id,
				 const LLSaleInfo sale_info);
	void sendAttach(U8 attachment_point);
	void sendDetach();
	void sendDropAttachment();
	void sendLink();
	void sendDelink();
#ifdef SEND_HINGES
	void sendHinge(U8 type);
	void sendDehinge();
#endif
	void sendSelect();

	static void registerObjectPropertiesFamilyRequest(const LLUUID& id);

	// Asks sim for creator, permissions, resources, etc.
	void requestObjectPropertiesFamily(LLViewerObject* object);

	static void processObjectProperties(LLMessageSystem* mesgsys,
										void** user_data);
	static void processObjectPropertiesFamily(LLMessageSystem* mesgsys,
											  void**);
	static void processForceObjectSelect(LLMessageSystem* msg, void**);

	void requestGodInfo();

	LL_INLINE const LLVector3d& getSelectionCenterGlobal() const
	{
		return mSelectionCenterGlobal;
	}

	void updateSelectionCenter();

	void pauseAssociatedAvatars();

	void updatePointAt();

	// Internal list maintenance functions. TODO: Make these private!
	void remove(std::vector<LLViewerObject*>& objects);
	void remove(LLViewerObject* object, S32 te = SELECT_ALL_TES,
				bool undoable = true);
	void removeAll();
	void addAsIndividual(LLViewerObject* object, S32 te = SELECT_ALL_TES,
						 bool undoable = true);
	void promoteSelectionToRoot();
	void demoteSelectionToIndividuals();

private:
	// Converts temporarily selected objects to full-fledged selections
	void convertTransient();

	ESelectType getSelectTypeForObject(LLViewerObject* object);
	void addAsFamily(std::vector<LLViewerObject*>& objects,
					 bool add_to_end = false);
	void generateSilhouette(LLSelectNode* nodep, const LLVector3& view_point);
	void updateSelectionSilhouette(LLObjectSelectionHandle object_handle,
								   S32& num_sils_genned,
								   std::vector<LLViewerObject*>& changed_objects);
	// Send one message to each region containing an object on selection list.
	void sendListToRegions(const std::string& message_name,
						   void (*pack_header)(void*),
						   void (*pack_body)(LLSelectNode*, void*),
						   void* user_data,
						   ESendType send_type);

	static void connectRefreshCachedSettingsSafe(const char* name);
	static void refreshCachedSettings();

	static void packAgentID(void*);
	static void packAgentAndSessionID(void* user_data);
	static void packAgentAndGroupID(void* user_data);
	static void packAgentAndSessionAndGroupID(void* user_data);
	static void packAgentIDAndSessionAndAttachment(void*);
	static void packAgentGroupAndCatID(void*);
	static void packDeleteHeader(void* userdata);
	static void packDeRezHeader(void* user_data);
	static void packObjectID(LLSelectNode* node, void*);
	static void packObjectIDAsParam(LLSelectNode* node, void*);
	static void packObjectIDAndRotation(LLSelectNode* node, void*);
	static void packObjectLocalID(LLSelectNode* node, void*);
	static void packObjectClickAction(LLSelectNode* node, void* data);
	static void packObjectIncludeInSearch(LLSelectNode* node, void* data);
	static void packObjectName(LLSelectNode* node, void* user_data);
	static void packObjectDescription(LLSelectNode* node, void* user_data);
	static void packObjectCategory(LLSelectNode* node, void* user_data);
	static void packObjectSaleInfo(LLSelectNode* node, void* user_data);
	static void packBuyObjectIDs(LLSelectNode* node, void* user_data);
	static void packDuplicate(LLSelectNode* node, void* duplicate_data);
	static void packDuplicateHeader(void*);
	static void packDuplicateOnRayHead(void* user_data);
	static void packPermissions(LLSelectNode* node, void* user_data);
	static void packDeselect(LLSelectNode* node, void* user_data);
	static void packMultipleUpdate(LLSelectNode* node, void* user_data);
	static void packPhysics(LLSelectNode* node, void* user_data);
	static void packShape(LLSelectNode* node, void* user_data);
	static void packOwnerHead(void* user_data);
#ifdef SEND_HINGES
	static void packHingeHead(void* user_data);
#endif
	static void packPermissionsHead(void* user_data);
	static void packGodlikeHead(void* user_data);
	static bool confirmDelete(const LLSD& notification, const LLSD& response,
							  LLObjectSelectionHandle handle);

	// Gets the first ID that matches test and whether or not all Ids are
	// identical in selected objects.
	void getFirst(LLSelectGetFirstTest* test);

	// Only used for PBR rendering, for now. HB
	void renderMeshSelection(LLSelectNode* nodep, LLViewerObject* objectp,
							 LLDrawable* drawablep, LLVOVolume* volp,
							 const LLColor4& color, bool no_hidden = false);
public:
	// Observer/callback support for when object selection changes or
	// properties are received/updated
	typedef boost::signals2::signal<void()> update_signal_t;
	update_signal_t mUpdateSignal;

	// Do we need to surround an object to pick it ?
	static bool		sRectSelectInclusive;
	// Do we show the radius of selected lights?
	static bool		sRenderLightRadius;

	static F32		sHighlightThickness;
	static F32		sHighlightUScale;
	static F32		sHighlightVScale;
	static F32		sHighlightAlpha;
	static F32		sHighlightAlphaTest;
	static F32		sHighlightUAnim;
	static F32		sHighlightVAnim;
	static LLColor4	sSilhouetteParentColor;
	static LLColor4	sSilhouetteChildColor;
	static LLColor4	sHighlightParentColor;
	static LLColor4	sHighlightChildColor;
	static LLColor4	sHighlightInspectColor;
	static LLColor4	sContextSilhouetteColor;

	U32				mRenderSelectionsPolicy;
	bool			mHideSelectedObjects;
	bool			mAllowSelectAvatar;
	bool			mDebugSelectMgr;
	bool			mEditLinkedParts;
	bool			mSelectOwnedOnly;
	bool			mSelectMovableOnly;

private:
	LLPointer<LLViewerTexture>				mSilhouetteImagep;
	LLObjectSelectionHandle					mSelectedObjects;
	LLObjectSelectionHandle					mHoverObjects;
	LLObjectSelectionHandle					mHighlightedObjects;
	std::set<LLPointer<LLViewerObject> >	mRectSelectedObjects;

	LLObjectSelection						mGridObjects;
	LLQuaternion							mGridRotation;
	LLVector3								mGridOrigin;
	LLVector3								mGridScale;
	EGridMode								mGridMode;

	// Diffuse, normal or specular, depending on editing mode:
	LLRender::eTexIndex						mTextureChannel;

	LLVector3d								mSelectionCenterGlobal;
	LLBBox									mSelectionBBox;

	LLVector3d								mLastSentSelectionCenterGlobal;

	// Camera position from last generation of selection silhouette:
	LLVector3d								mLastCameraPos;

	LLBBox									mSavedSelectionBBox;

	LLFrameTimer							mEffectsTimer;

	std::vector<LLAnimPauseRequest>			mPauseRequests;

	// Render TE
	bool									mTEMode;

	// Do we send the selection center name value and do we animate this
	// selection ?
	bool									mShowSelection;
	// Do we render the silhouette ?
	bool									mRenderSilhouettes;

	bool									mForceSelection;

	static uuid_list_t						sObjectPropertiesFamilyRequests;
};

// Utilities
void dialog_refresh_all();		// Update subscribers to the selection list


// Templates
//-----------------------------------------------------------------------------
// getSelectedTEValue
//-----------------------------------------------------------------------------
template <typename T> bool LLObjectSelection::getSelectedTEValue(LLSelectedTEGetFunctor<T>* func,
																 T& res)
{
	bool have_first = false;
	bool have_selected = false;
	T selected_value = T();

	// Now iterate through all TEs to test for sameness
	bool identical = true;
	for (iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		S32 selected_te = -1;
		if (object == getPrimaryObject())
		{
			selected_te = node->getLastSelectedTE();
		}
		for (S32 te = 0, count = object->getNumTEs(); te < count; ++te)
		{
			if (!node->isTESelected(te))
			{
				continue;
			}
			T value = func->get(object, te);
			if (!have_first)
			{
				have_first = true;
				if (!have_selected)
				{
					selected_value = value;
				}
			}
			else
			{
				if (value != selected_value)
				{
					identical = false;
				}
				if (te == selected_te)
				{
					selected_value = value;
					have_selected = true;
				}
			}
		}
		if (!identical && have_selected)
		{
			break;
		}
	}
	if (have_first || have_selected)
	{
		res = selected_value;
	}
	return identical;
}

// Templates
//-----------------------------------------------------------------------------
// isMultipleTEValue iterate through all TEs and test for uniqueness
// with certain return value ignored when performing the test.
// E.g. when testing if the selection has a unique non-empty home URL you can
// set ignore_value = "" and it will only compare among the non-empty home URLs
// and ignore the empty ones.
//-----------------------------------------------------------------------------
template <typename T> bool LLObjectSelection::isMultipleTEValue(LLSelectedTEGetFunctor<T>* func,
																const T& ignore_value)
{
	bool have_first = false;
	T selected_value = T();

	// Now iterate through all TEs to test for sameness
	bool unique = true;
	for (iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		for (S32 te = 0, count = object->getNumTEs(); te < count; ++te)
		{
			if (!node->isTESelected(te))
			{
				continue;
			}
			T value = func->get(object, te);
			if (value == ignore_value)
			{
				continue;
			}
			if (!have_first)
			{
				have_first = true;
			}
			else
			{
				if (value != selected_value)
				{
					unique = false;
					return !unique;
				}
			}
		}
	}
	return !unique;
}

extern LLSelectMgr gSelectMgr;

#endif
