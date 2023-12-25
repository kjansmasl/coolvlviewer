/**
 * @file llfloatermodelpreview.h
 * @brief LLFloaterModelPreview class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
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

#ifndef LL_LLFLOATERMODELPREVIEW_H
#define LL_LLFLOATERMODELPREVIEW_H

#include "llcontrol.h"
#include "llfloater.h"
#include "llhandle.h"
#include "llmodelloader.h"
#include "llquaternion.h"
#include "llthread.h"

#include "lldynamictexture.h"
#include "llmeshrepository.h"
#include "llviewermenu.h"

class DAE;
class daeElement;
class domProfile_COMMON;
class domInstance_geometry;
class domNode;
class domTranslate;
class domController;
class domMesh;
class domSkin;
class LLComboBox;
class LLVOAvatar;
class LLVertexBuffer;
class LLModelPreview;
class LLFloaterModelPreview;
class LLScrollListCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextEditor;

class LLUploadPermissionsObserver
{
protected:
	LOG_CLASS(LLUploadPermissionsObserver);

public:
	LLUploadPermissionsObserver()			{ mUploadPermObserverHandle.bind(this); }
	virtual ~LLUploadPermissionsObserver()	{}

	virtual void onPermissionsReceived(const LLSD& result) = 0;
	virtual void setPermissonsErrorStatus(S32 status, const std::string& reason) = 0;

	LLHandle<LLUploadPermissionsObserver> getPermObserverHandle() const
	{
		return mUploadPermObserverHandle;
	}

protected:
	LLRootHandle<LLUploadPermissionsObserver> mUploadPermObserverHandle;
};

class LLFloaterModelUploadBase : public LLFloater,
								 public LLUploadPermissionsObserver,
								 public LLWholeModelFeeObserver,
								 public LLWholeModelUploadObserver
{
protected:
	LOG_CLASS(LLFloaterModelUploadBase);

public:
	LLFloaterModelUploadBase();

	~LLFloaterModelUploadBase() override	{}

	void setPermissonsErrorStatus(S32 status,
								  const std::string& reason) override = 0;

	void onPermissionsReceived(const LLSD& result) override = 0;

	void onModelPhysicsFeeReceived(const LLSD& result,
								   std::string upload_url) override = 0;

	void setModelPhysicsFeeErrorStatus(S32 status, const std::string& reason,
									   const LLSD& result) override = 0;

	void onModelUploadSuccess() override	{}

	void onModelUploadFailure() override	{}

protected:
	// Requests agent's permissions to upload model
	void requestAgentUploadPermissions();

	void requestUploadPermCoro(std::string url,
							   LLHandle<LLUploadPermissionsObserver> handle);
protected:
	std::string	mUploadModelUrl;
	bool		mHasUploadPerm;
};

class LLFloaterModelPreview final
:	public LLFloaterModelUploadBase,
	public LLFloaterSingleton<LLFloaterModelPreview>
{
	friend class LLModelPreview;
	friend class LLPhysicsDecomp;
	friend class LLUISingleton<LLFloaterModelPreview,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterModelPreview);

public:
	class DecompRequest : public LLPhysicsDecomp::Request
	{
	public:
		S32 mContinue;
		LLPointer<LLModel> mModel;

		DecompRequest(const std::string& stage, LLModel* mdl);
		virtual S32 statusCallback(const char* status, S32 p1, S32 p2);
		virtual void completed();
	};

	~LLFloaterModelPreview() override;

	bool postBuild() override;

	void onOpen() override;

	void initModelPreview();

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;

	static void onMouseCaptureLostModelPreview(LLMouseHandler*);
	LL_INLINE static void setUploadAmount(S32 amount)		{ sUploadAmount = amount; }

	static LLModelPreview* getModelPreview();

	void setDetails(F32 x, F32 y, F32 z);

	void refresh() override;

	void loadModel(S32 lod);
	void loadModel(S32 lod, const std::string& file_name,
				   bool force_disable_slm = false);

	void clearSkinningInfo();
	void updateSkinningInfo(bool highlight_overrides);

	bool isViewOptionChecked(const LLSD& userdata);
	bool isViewOptionEnabled(const LLSD& userdata);
	void setViewOptionEnabled(const char* option, bool enabled);
	void enableViewOption(const char* option);
	void disableViewOption(const char* option);

	// Shows warning message if agent has no permissions to upload model
	void onPermissionsReceived(const LLSD& result) override;

	// Called when error occurs during permissions request
	void setPermissonsErrorStatus(S32 status,
								  const std::string& reason) override;

	void onModelPhysicsFeeReceived(const LLSD& result,
								   std::string upload_url) override;
	void handleModelPhysicsFeeReceived();
	void setModelPhysicsFeeErrorStatus(S32 status, const std::string& reason,
									   const LLSD& result) override;

	void onModelUploadSuccess() override;

	void onModelUploadFailure() override;

	bool isModelUploadAllowed();

protected:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance(), or implicitely via getInstance().
	LLFloaterModelPreview(const LLSD&);

	bool prepareToLoadModel(S32 lod);

	static void onTabChanged(void* userdata, bool);

	static void onUpload(void* userdata);
	static void onReset(void* userdata);

	static void	onImportScaleCommit(LLUICtrl*, void* userdata);
	static void	onPelvisOffsetCommit(LLUICtrl*, void* userdata);
	static void	onUploadJointsCommit(LLUICtrl*, void* userdata);
	static void	onUploadSkinCommit(LLUICtrl*, void* userdata);

	static void onJointListSelection(LLUICtrl*, void* userdata);

	// NOTE: for these, userdata bears the LOD level instead of "this"
	static void onBrowseLOD(void* userdata);
	static void	onPreviewLODCommit(LLUICtrl* ctrl, void* userdata);
	static void onLoDSourceCommit(LLUICtrl*, void* userdata);
	static void onLODParamCommit(LLUICtrl*, void* userdata);
	static void onLODParamCommitEnforceTriLimit(LLUICtrl*, void* userdata);

	static void	onGenerateNormalsCommit(LLUICtrl*, void* userdata);

	static void toggleGenerateNormals(LLUICtrl*, void* userdata);

	static void	onExplodeCommit(LLUICtrl*, void* userdata);

	static void	onAutoFillCommit(LLUICtrl*, void* userdata);

	static void toggleCalculateButtonCallBack(LLUICtrl*, void* userdata);

	static void onClickTextLOD(void* userdata);

	static void onClickCalculateBtn(void* userdata);

	static void onViewOptionChecked(LLUICtrl* ctrl, void* userdata);

	// For this one, userdata bears a LLCDStageData pointer instead of "this"
	static void onPhysicsStageExecute(LLUICtrl*, void* userdata);

	static void onPhysicsParamCommit(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsStageCancel(void* userdata);
	static void onCancel(void* userdata);

	static void onPhysicsBrowse(void* userdata);
	static void onPhysicsUseLOD(LLUICtrl*, void* userdata);
#if 0	// Not implemented (not needed/supported with HACD)
	static void onPhysicsOptimize(void* userdata);
	static void onPhysicsDecomposeBack(void* userdata);
	static void onPhysicsSimplifyBack(void* userdata);
#endif

	static void onClickValidateURL(void* userdata);

	void draw() override;

	void initDecompControls();

	void setStatusMessage(const std::string& msg);

	void addMessageToLog(const std::string& line, const LLSD& args, S32 lod,
						 bool flash = false);
	void addLineToLog(const std::string& line, bool flash = false);
	void clearLog();

private:
	// Toggles between "Calculate weights & fee" and "Upload" buttons.
	void toggleCalculateButton(bool visible);
	// Calls the above after clearing mModelPhysicsFee.
	void modelUpdated(bool visible);

	// Resets display options of model preview to their defaults.
	void resetDisplayOptions();

	void createSmoothComboBox(LLComboBox* combo_box, F32 min, F32 max);

protected:
	LLModelPreview*						mModelPreview;

	LLPhysicsDecomp::decomp_params_t	mDecompParams;

	S32									mLastMouseX;
	S32									mLastMouseY;
	LLRect								mPreviewRect;
	static S32							sUploadAmount;

	std::set<LLPointer<DecompRequest> >	mCurRequest;
	std::string							mStatusMessage;

	// Use "disabled" as false by default
	std::map<std::string, bool>			mViewOptionDisabled;

	// stores which lod mode each LOD is using
	// 0 - load from file
	// 1 - auto generate
	// 2 - use LoD above
	S32									mLODMode[4];

	LLMutex*							mStatusLock;

	LLSD								mModelPhysicsFee;

private:
	LLTabContainer*						mTabContainer;
	LLPanel*							mModifiersPanel;
	LLPanel*							mLogPanel;
	LLButton*							mUploadBtn;
	LLButton*							mCalculateBtn;
	LLScrollListCtrl*					mJointsList;
	LLScrollListCtrl*					mJointsOverrides;
	LLTextBox*							mConflictsText;
	LLTextBox*							mOverridesLabel;
	LLTextEditor*						mUploadLogText;

	// Account upload permission validation URL
	std::string							mValidateURL;

	std::string							mSelectedJointName;

	struct JointOverrideData
	{
		JointOverrideData()
		:	mHasConflicts(false)
		{
		}

		// Models with overrides
		std::map<std::string, LLVector3>	mPosOverrides;
		// Models without overrides
		std::set<std::string>				mModelsNoOverrides;
		bool								mHasConflicts;
	};
	typedef std::map<std::string, JointOverrideData> overrides_map_t;
	overrides_map_t						mJointOverrides[LLModel::NUM_LODS];

	// true while waiting for a reply to a fee request
	bool								mSentFeeRequest;
	// true while waiting for the end of an upload
	bool 								mSentUploadRequest;
	// true when HACD library is detected.
	bool mLibIsHACD;
};

class LLModelPreview final : public LLViewerDynamicTexture, public LLMutex
{
	friend class LLModelLoader;
	friend class LLFloaterModelPreview;
	friend class LLFloaterModelPreview::DecompRequest;
	friend class LLPhysicsDecomp;

protected:
	LOG_CLASS(LLModelPreview);

public:
	typedef enum
	{
		LOD_FROM_FILE = 0,
		GENERATE,
		// Automatically selects method based on model or face:
		MESH_OPTIMIZER_AUTO,
		MESH_OPTIMIZER_PRECISE,
		MESH_OPTIMIZER_SLOPPY,
		USE_LOD_ABOVE
	} eLoDMode;

	LLModelPreview(S32 width, S32 height, LLFloaterModelPreview* fmp);
	~LLModelPreview() override;

	void resetPreviewTarget();
	void setPreviewTarget(F32 distance);
	LL_INLINE void setTexture(U32 name)						{ mTextureName = name; }

	void setPhysicsFromLOD(S32 lod);
	void update();
	void genBuffers(S32 lod, bool skinned);
	void clearBuffers();

	LL_INLINE void refresh()								{ mNeedsUpdate = true; }
	// LLViewerDynamicTexture override
	LL_INLINE bool needsRender() override					{ return mNeedsUpdate; }
	bool render() override;

	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_amt);
	void pan(F32 right, F32 up);

	void setPreviewLOD(S32 lod);
	void clearModel(S32 lod);
	void getJointAliases(JointMap& joint_map);
	void loadModel(std::string filename, S32 lod,
				   bool force_disable_slm = false,
				   bool allow_preprocess = true);
	void loadModelCallback(S32 lod);

	LL_INLINE bool lodsReady()								{ return !mGenLOD && mLodsQuery.empty(); }
	LL_INLINE void queryLODs()								{ mGenLOD = true; }

	// Returns false if GLOD failed to optimize the LOD.
	bool genGlodLODs(S32 which_lod = -1, U32 decimation = 3,
					 bool enforce_tri_limit = false);

	void genMeshOptimizerLODs(S32 which_lod, S32 meshopt_mode,
							  U32 decimation = 3,
							  bool enforce_tri_limit = false);

	void generateNormals();
	void restoreNormals();
	void updateDimentionsAndOffsets();
	void rebuildUploadData();
	void saveUploadData(bool save_skinweights, bool save_joint_poisitions,
						bool lock_scale_if_joint_pos);
	void saveUploadData(const std::string& filename, bool save_skinweights,
						bool save_joint_poisitions,
						bool lock_scale_if_joint_pos);
	void clearIncompatible(S32 lod);
	void updateStatusMessages();
	void updateLodControls(S32 lod);
	void clearGLODGroup();
	void onLODParamCommit(S32 lod, bool enforce_tri_limit);
	void addEmptyFace(LLModel* pTarget);

	LL_INLINE bool getModelPivot() const					{ return mHasPivot; }
	LL_INLINE void setHasPivot(bool val)					{ mHasPivot = val; }
	LL_INLINE void setModelPivot(const LLVector3& pivot)	{ mModelPivot = pivot; }

	// Is a rig valid so that it can be used as a criteria for allowing for
	// uploading of joint positions
	LL_INLINE bool isRigValidForJointPositionUpload() const { return mRigValidJointUpload; }

	// Accessors for the legacy rigs
	LL_INLINE bool isLegacyRigValid() const					{ return mLegacyRigFlags == 0; }
	LL_INLINE U32 getLegacyRigFlags() const					{ return mLegacyRigFlags; }

protected:
	typedef boost::signals2::signal<void (F32 x, F32 y, F32 z)> details_sig_t;
	boost::signals2::connection setDetailsCallback(const details_sig_t::slot_type& cb)
	{
		return mDetailsSignal.connect(cb);
	}

	typedef boost::signals2::signal<void ()> loaded_sig_t;
	boost::signals2::connection setModelLoadedCallback(const loaded_sig_t::slot_type& cb)
	{
		return mModelLoadedSignal.connect(cb);
	}

	typedef boost::signals2::signal<void (bool)> updated_sig_t;
	boost::signals2::connection setModelUpdatedCallback(const updated_sig_t::slot_type& cb)
	{
		return mModelUpdatedSignal.connect(cb);
	}

	LL_INLINE void setLoadState(U32 state)					{ mLoadState = state; }
	LL_INLINE U32 getLoadState() const						{ return mLoadState; }

	bool matchMaterialOrder(LLModel* lod, LLModel* ref,
							S32& ref_face_cnt, S32& model_face_cnt);

	static void	textureLoadedCallback(bool success,
									  LLViewerFetchedTexture* src_vi,
									  LLImageRaw* src, LLImageRaw* src_aux,
									  S32 discard_level, bool is_final,
									  void* userdata);

	static bool lodQueryCallback();

	static void loadedCallback(LLModelLoader::scene& scene,
							   LLModelLoader::model_list& model_list,
							   S32 lod, void* userdata);
	static void stateChangedCallback(U32 state, void* userdata);

	static LLJoint* lookupJointByName(const std::string&, void* userdata);
	static U32 loadTextures(LLImportMaterial& material, void* userdata);

private:
	// Utility method for controller vertex compare
	bool verifyCount(int expected, int result);

	// Creates the dummy avatar for the preview window
	void createPreviewAvatar();

	void renderGroundPlane(F32 z_offset);

	// Count amount of original models, excluding sub-models
	static U32 countRootModels(LLModelLoader::model_list models);

	enum
	{
		MESH_OPTIMIZER_FULL,
		MESH_OPTIMIZER_NO_NORMALS,
		MESH_OPTIMIZER_NO_UVS,
		MESH_OPTIMIZER_NO_TOPOLOGY,
	};

	// Methods using the meshoptimizer library, returning the reached
	// simplification ratio, or -1.f on failure to simplify the model.
	F32 genMeshOptimizerPerModel(LLModel* base_model, LLModel* target_model,
								 F32 indices_ratio, F32 error_threshold,
								 S32 simplification_mode);
	F32 genMeshOptimizerPerFace(LLModel* base_model, LLModel* target_model,
								U32 face_idx, F32 indices_ratio,
								F32 error_threshold, S32 simplification_mode);

protected:
	LLFloaterModelPreview*  mFMP;
	LLModelLoader*			mModelLoader;

	LLModel*				mDefaultPhysModel;

	LLPointer<LLVOAvatar>	mPreviewAvatar;
	LLVector3				mGroundPlane[4];

	U32         			mTextureName;
	F32						mCameraDistance;
	F32						mCameraYaw;
	F32						mCameraPitch;
	F32						mCameraZoom;
	S32						mPreviewLOD;
	U32						mLoadState;

	S32						mPhysicsSearchLOD;

	U32						mGroup;

	// Amount of triangles in original (base) model
	U32						mMaxTriangleLimit;

	F32						mPelvisZOffset;

	LLVector3				mCameraOffset;
	LLVector3				mPreviewTarget;
	LLVector3				mPreviewScale;
	LLVector3				mModelPivot;

	std::string				mLODFile[LLModel::NUM_LODS];

	typedef std::map<std::string, bool> view_option_map_t;
	view_option_map_t		mViewOption;

	U32						mLegacyRigFlags;

	bool					mRigValidJointUpload;
	bool					mFirstSkinUpdate;
	bool        			mNeedsUpdate;
	bool					mDirty;
	bool					mModelNoErrors;
	bool					mGenLOD;
	bool					mWarnPhysModel;
	bool					mLoading;
	bool					mResetJoints;
	bool					mHasPivot;
	bool					mLastJointUpdate;
	bool					mHasDegenerate;

	// GLOD object parameters (must rebuild object if these change)
	bool					mLODFrozen;
	F32						mBuildShareTolerance;
	U32						mBuildQueueMode;
	U32						mBuildOperator;
	U32						mBuildBorderMode;
	U32						mRequestedLoDMode[LLModel::NUM_LODS];
	S32						mRequestedTriangleCount[LLModel::NUM_LODS];
	F32						mRequestedErrorThreshold[LLModel::NUM_LODS];
	U32						mRequestedBuildOperator[LLModel::NUM_LODS];
	U32						mRequestedQueueMode[LLModel::NUM_LODS];
	U32						mRequestedBorderMode[LLModel::NUM_LODS];
	F32						mRequestedShareTolerance[LLModel::NUM_LODS];
	F32						mRequestedCreaseAngle[LLModel::NUM_LODS];

	LLModelLoader::scene	mScene[LLModel::NUM_LODS];
	LLModelLoader::scene	mBaseScene;

	LLModelLoader::model_list	mModel[LLModel::NUM_LODS];
	LLModelLoader::model_list	mBaseModel;

	typedef std::vector<LLVolumeFace> v_LLVolumeFace_t;
	typedef std::vector<v_LLVolumeFace_t> vv_LLVolumeFace_t;
	vv_LLVolumeFace_t		mModelFacesCopy[LLModel::NUM_LODS];
	vv_LLVolumeFace_t		mBaseModelFacesCopy;

	std::vector<S32>		mLodsQuery;
	std::vector<S32>		mLodsWithParsingError;

	typedef std::map<LLPointer<LLModel>, U32> model_object_map_t;
	model_object_map_t		mObject;

	LLMeshUploadThread::instance_list_t	mUploadData;
	std::set<LLViewerFetchedTexture*>	mTextureSet;

	// Map of vertex buffers to models (one vertex buffer in vector per face in
	// model).
	typedef std::map<LLModel*,
					 std::vector<LLPointer<LLVertexBuffer> > > model_vb_map_t;
	model_vb_map_t			mVertexBuffer[LLModel::NUM_LODS + 1];

	details_sig_t			mDetailsSignal;
	loaded_sig_t			mModelLoadedSignal;
	updated_sig_t			mModelUpdatedSignal;

	JointNameSet			mJointsFromNode;
	JointTransformMap		mJointTransformMap;

	LLCachedControl<bool>	mImporterDebug;
};

#endif  // LL_LLFLOATERMODELPREVIEW_H
