/**
 * @file llprefsgraphics.cpp
 * @brief Graphics preferences for the preferences floater
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

#include "llviewerprecompiledheaders.h"

#include <regex>

#include "llprefsgraphics.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "llradiogroup.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include "llfeaturemanager.h"
#include "llfloaterpreference.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llstartup.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For send_agent_update()
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvosky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llworld.h"

static bool sNeedsRefresh = false;

class LLPrefsGraphicsImpl final : public LLPanel
{
	friend class LLPreferenceCore;

public:
	LLPrefsGraphicsImpl();
	~LLPrefsGraphicsImpl() override;

	bool postBuild() override;

	void draw() override;

	void refresh() override;	// Refresh enable/disable

	void apply();				// Apply the changed values.
	void applyResolution();
	void applyWindowSize();
	void cancel();

private:
	void setCloudsMaxAlt();
	void initWindowSizeControls();
	bool extractSizeFromString(const std::string& instr, U32& width,
							   U32& height);

	void refreshEnabledState();

	static void onTabChanged(void* data, bool);

	// When the quality radio buttons are changed
	static void onChangeQuality(LLUICtrl* ctrl, void* data);
	// When the custom settings box is clicked
	static void onChangeCustom(LLUICtrl* ctrl, void* data);

	static void onOpenHelp(void* data);
	static void onCommitAutoDetectAspect(LLUICtrl* ctrl, void* data);
	static void onKeystrokeAspectRatio(LLLineEditor* caller, void* data);
	static void onSelectAspectRatio(LLUICtrl*, void*);
	static void onCommitWindowedMode(LLUICtrl* ctrl, void* data);
	static void updateSliderText(LLUICtrl* ctrl, void* data);
	static void updateMeterText(LLUICtrl* ctrl, void* data);
	static void onClassicClouds(LLUICtrl* ctrl, void* data);
	static void onCommitNeedsRefresh(LLUICtrl*, void* data);
	static void onCommitAvatarPhysics(LLUICtrl* ctrl, void* data);
	static void onCommitMaxNonImpostors(LLUICtrl* ctrl, void* data);

	// Callback for defaults
	static void setHardwareDefaults(void* data);

	// Helper method
	static void fractionFromDecimal(F32 decimal_val, S32& numerator,
									S32& denominator);

private:
	typedef boost::signals2::connection connection_t;
	connection_t	mCommitSignal;

	LLTabContainer*	mTabContainer;

	// Aspect ratio sliders and boxes
	LLComboBox*		mCtrlFullScreen;		// Fullscreen resolution
	LLCheckBoxCtrl*	mCtrlAutoDetectAspect;	// Auto-detect aspect ratio
	LLComboBox*		mCtrlAspectRatio;		// User provided aspect ratio

	LLCheckBoxCtrl*	mCtrlWindowed;			// Windowed mode
	LLComboBox*		mCtrlWindowSize;		// Window size for windowed mode
	LLCheckBoxCtrl*	mCtrlBenchmark;			// Benchmark GPU

	/// Performance radio group
	LLSliderCtrl*	mCtrlSliderQuality;

	// Performance sliders and boxes
	LLSliderCtrl*	mCtrlDrawDistance;		// Draw distance slider
	LLSliderCtrl*	mCtrlLocalLightCount;	// Mas local lights count
	LLSliderCtrl*	mCtrlGlowResolution;	// Glow map resolution
	LLSliderCtrl*	mCtrlLODFactor;			// LOD for volume objects
	LLSliderCtrl*	mCtrlFlexFactor;		// Timeslice for flexible objects
	LLSliderCtrl*	mCtrlTreeFactor;		// Control tree cutoff distance
	LLSliderCtrl*	mCtrlAvatarFactor;		// LOD for avatars
	LLSliderCtrl*	mCtrlTerrainFactor;		// LOD for terrain
	LLSliderCtrl*	mCtrlSkyFactor;			// LOD for terrain
	LLSliderCtrl*	mCtrlMaxParticle;		// Max Particle

	LLCheckBoxCtrl*	mCtrlGlow;
	LLCheckBoxCtrl*	mCtrlDeferredEnable;
	LLCheckBoxCtrl*	mCtrlAvatarCloth;
	LLCheckBoxCtrl*	mCtrlClassicClouds;
	LLCheckBoxCtrl*	mCtrlPBREnable;
	LLSliderCtrl*	mCtrlExposure;
	LLCheckBoxCtrl*	mCtrlTransparentWater;
	LLCheckBoxCtrl*	mCtrlScreenSpaceReflections;

	LLSpinCtrl*		mRenderGlowStrength;
	LLSpinCtrl*		mSpinCloudsAltitude;

	LLComboBox*		mComboRenderShadowDetail;
	LLComboBox*		mComboWaterReflectionType;
	LLComboBox*		mComboReflectionDetail;
	LLComboBox*		mComboReflectionLevel;
	LLCheckBoxCtrl*	mCtrlSMAA;
	LLCheckBoxCtrl*	mCtrlSharpen;

	LLTextBox*		mAspectRatioLabel1;
	LLTextBox*		mDisplayResLabel;
	LLTextBox*		mFullScreenInfo;
	LLTextBox*		mWindowSizeLabel;

	LLTextBox*		mDrawDistanceMeterText1;
	LLTextBox*		mDrawDistanceMeterText2;

	LLTextBox*		mLODFactorText;
	LLTextBox*		mFlexFactorText;
	LLTextBox*		mTreeFactorText;
	LLTextBox*		mAvatarFactorText;
	LLTextBox*		mTerrainFactorText;
	LLTextBox*		mSkyFactorText;
	LLTextBox*		mGlowResolutionText;
	LLTextBox*		mClassicCloudsText;

	// GPU/GL features sub-tab
	LLCheckBoxCtrl*	mCtrlOcclusion;

	// Avatar rendering sub-tab
	LLSliderCtrl*	mCtrlMaxNonImpostors;
	LLSliderCtrl*	mCtrlMaximumComplexity;
	LLSliderCtrl*	mCtrlSurfaceAreaLimit;
	LLSliderCtrl*	mCtrlGeometryBytesLimit;

	F32				mAspectRatio;

	// Performance value holders for cancel
	S32				mQualityPerformance;

	// Renderer settings sub-tab
	U32				mWaterReflectionType;
	U32				mWaterMaxAltitude;
	S32				mAvatarMode;
	S32				mClassicCloudsAvgAlt;
	U32				mLocalLightCount;
	S32				mTerrainDetail;
	U32				mRenderShadowDetail;
	U32				mReflectionProbeDetail;
	U32				mReflectionProbeLevel;
	F32				mRenderFarClip;
	F32				mPrimLOD;
	F32				mMeshLODBoost;
	F32				mFlexLOD;
	F32				mTreeLOD;
	F32				mAvatarLOD;
	F32				mTerrainLOD;
	S32				mSkyLOD;
	S32				mParticleCount;
	S32				mPostProcess;
	F32				mGlowStrength;
	F32				mExposure;
	U32				mFSAASamples;

	bool			mFullScreen;
	bool			mGlow;
	bool			mRenderTransparentWater;
	bool			mScreenSpaceReflections;
	bool			mUseSMAAShader;
	bool			mUseSharpenShader;

	bool			mFSAutoDetectAspect;
#if !LL_LINUX	// Irrelevant setting for Linux
	bool			mRenderHiDPI;
#endif
	bool			mRenderPBR;
	bool			mRenderDeferred;
	bool			mAvatarCloth;
	bool			mUseClassicClouds;
	bool			mCanDoCloth;
	bool			mCanDoDeferred;

	// GPU/GL features sub-tab
	bool			mRenderGLCoreProfile;
	bool			mUseAniso;
	bool			mDisableVRAMCheck;
	bool			mCompressTextures;
	bool			mRenderUseRGBA16ATI;
	S32				mGLWorkerThreads;
	F32				mGamma;
	U32				mVRAMOverride;
	S32				mTextureMem;
	U32				mMaxBoundTexMem;
	U32				mRenderCompressThreshold;

	// Avatars rendering sub-tab
	U32				mNonImpostors;
	U32				mNonImpostorsPuppets;
	U32				mRenderAvatarMaxComplexity;
	F32				mRenderAutoMuteSurfaceAreaLimit;
	U32				mRenderAutoMuteMemoryLimit;
	F32				mRenderAvatarPhysicsLODFactor;
	bool			mAvatarPhysics;
	bool			mAlwaysRenderFriends;
	bool			mColoredJellyDolls;

	bool			mFirstRun;
};

constexpr S32 ASPECT_RATIO_STR_LEN = 100;

LLPrefsGraphicsImpl::LLPrefsGraphicsImpl()
:	mFirstRun(true)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_graphics.xml");
}

bool LLPrefsGraphicsImpl::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("graphics_tabs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("Renderer settings");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("GPU/GL features");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Avatars rendering");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	// Setup graphic card driver capabilities
	mCanDoCloth = gFeatureManager.isFeatureAvailable("RenderAvatarCloth");
	mCanDoDeferred = gFeatureManager.isFeatureAvailable("RenderDeferred");

	// Return to default values
	childSetAction("Defaults", setHardwareDefaults, this);

	// Help button
	childSetAction("GraphicsPreferencesHelpButton", onOpenHelp, this);

	// Resolution

	// Radio set for fullscreen size
	mCtrlWindowed = getChild<LLCheckBoxCtrl>("windowed mode");
	mCtrlWindowed->setCommitCallback(onCommitWindowedMode);
	mCtrlWindowed->setCallbackUserData(this);

	mAspectRatioLabel1 = getChild<LLTextBox>("AspectRatioLabel1");
	mFullScreenInfo = getChild<LLTextBox>("FullScreenInfo");
	mDisplayResLabel = getChild<LLTextBox>("DisplayResLabel");

	S32 num_resolutions = 0;
	LLWindow::LLWindowResolution* resolutions =
		gWindowp->getSupportedResolutions(num_resolutions);

	S32 fullscreen_mode = num_resolutions - 1;

	mCtrlFullScreen = getChild<LLComboBox>("fullscreen combo");

	LLUIString resolution_label = getString("resolution_format");

	for (S32 i = 0; i < num_resolutions; ++i)
	{
		resolution_label.setArg("[RES_X]",
								llformat("%d", resolutions[i].mWidth));
		resolution_label.setArg("[RES_Y]",
								llformat("%d", resolutions[i].mHeight));
		mCtrlFullScreen->add(resolution_label, ADD_BOTTOM);
	}

	bool want_full_screen = gSavedSettings.getBool("FullScreen");
	U32 width, height;
	gViewerWindowp->getTargetWindow(want_full_screen, width, height);
	if (want_full_screen)
	{
		fullscreen_mode = 0;
		for (S32 i = 0; i < num_resolutions; ++i)
		{
			if (width == (U32)resolutions[i].mWidth &&
				height == (U32)resolutions[i].mHeight)
			{
				fullscreen_mode = i;
			}
		}
		mCtrlFullScreen->setCurrentByIndex(fullscreen_mode);
		mCtrlWindowed->set(false);
		mCtrlFullScreen->setVisible(true);
	}
	else
	{
		// Set to windowed mode
		mCtrlWindowed->set(true);
		mCtrlFullScreen->setCurrentByIndex(0);
		mCtrlFullScreen->setVisible(false);
	}

	initWindowSizeControls();

	if (gSavedSettings.getBool("FullScreenAutoDetectAspectRatio"))
	{
		mAspectRatio = gViewerWindowp->getDisplayAspectRatio();
	}
	else
	{
		mAspectRatio = gSavedSettings.getF32("FullScreenAspectRatio");
	}

	S32 numerator = 0;
	S32 denominator = 0;
	fractionFromDecimal(mAspectRatio, numerator, denominator);

	LLUIString aspect_ratio_text = getString("aspect_ratio_text");
	if (numerator != 0)
	{
		aspect_ratio_text.setArg("[NUM]", llformat("%d",  numerator));
		aspect_ratio_text.setArg("[DEN]", llformat("%d",  denominator));
	}
	else
	{
		aspect_ratio_text = llformat("%.3f", mAspectRatio);
	}

	mCtrlAspectRatio = getChild<LLComboBox>("aspect_ratio");
	mCtrlAspectRatio->setTextEntryCallback(onKeystrokeAspectRatio);
	mCtrlAspectRatio->setCommitCallback(onSelectAspectRatio);
	mCtrlAspectRatio->setCallbackUserData(this);
	// Add default aspect ratios
	mCtrlAspectRatio->add(aspect_ratio_text, &mAspectRatio, ADD_TOP);
	mCtrlAspectRatio->setCurrentByIndex(0);

	mCtrlAutoDetectAspect = getChild<LLCheckBoxCtrl>("aspect_auto_detect");
	mCtrlAutoDetectAspect->setCommitCallback(onCommitAutoDetectAspect);
	mCtrlAutoDetectAspect->setCallbackUserData(this);

#if LL_LINUX
	// HiDPI (Retina) mode for macOS or UI scaling under Windows 10. Irrelevant
	// to Linux.
	childHide("hi_dpi_check");
#endif

	// Radio performance box
	mCtrlSliderQuality = getChild<LLSliderCtrl>("QualityPerformanceSelection");
	mCtrlSliderQuality->setSliderMouseUpCallback(onChangeQuality);
	mCtrlSliderQuality->setCallbackUserData(this);

	mCtrlBenchmark = getChild<LLCheckBoxCtrl>("benchmark_gpu_check");

	// Glow
	mCtrlGlow = getChild<LLCheckBoxCtrl>("RenderGlowCheck");
	mCtrlGlow->setCommitCallback(onCommitNeedsRefresh);
	mCtrlGlow->setCallbackUserData(this);
	mRenderGlowStrength = getChild<LLSpinCtrl>("glow_strength");
	mCtrlGlowResolution = getChild<LLSliderCtrl>("GlowResolutionPow");
	mGlowResolutionText = getChild<LLTextBox>("GlowResolutionText");
	mCtrlGlowResolution->setCommitCallback(updateSliderText);
	mCtrlGlowResolution->setCallbackUserData(mGlowResolutionText);

	// Water opacity or reflections
	mComboWaterReflectionType = getChild<LLComboBox>("WaterReflectionTypeCombo");
	mComboWaterReflectionType->setCurrentByIndex(gSavedSettings.getU32("RenderWaterReflectionType"));
	mCtrlTransparentWater = getChild<LLCheckBoxCtrl>("TransparentWaterCheck");

	// PBR reflections
	mCtrlScreenSpaceReflections = getChild<LLCheckBoxCtrl>("SSRCheck");
	mComboReflectionDetail = getChild<LLComboBox>("ReflectionDetailCombo");
	mComboReflectionDetail->setCurrentByIndex(gSavedSettings.getU32("RenderReflectionProbeDetail"));
	mComboReflectionLevel = getChild<LLComboBox>("ReflectionLevelCombo");
	mComboReflectionLevel->setCurrentByIndex(gSavedSettings.getU32("RenderReflectionProbeLevel"));

	// Avatar shader
	mCtrlAvatarCloth = getChild<LLCheckBoxCtrl>("AvatarCloth");

	// Deferred rendering
	mCtrlDeferredEnable = getChild<LLCheckBoxCtrl>("RenderDeferred");
	mCtrlDeferredEnable->setCommitCallback(onCommitNeedsRefresh);
	mCtrlDeferredEnable->setCallbackUserData(this);

	mComboRenderShadowDetail =
		getChild<LLComboBox>("RenderShadowDetailCombo");
	mComboRenderShadowDetail->setCurrentByIndex(gSavedSettings.getU32("RenderShadowDetail"));

	mCtrlSMAA = getChild<LLCheckBoxCtrl>("smaa");
	mCtrlSharpen = getChild<LLCheckBoxCtrl>("sharpen");

	mCtrlPBREnable = getChild<LLCheckBoxCtrl>("UsePBRCheck");
	mCtrlExposure = getChild<LLSliderCtrl>("RenderExposureCtrl");

	// Object detail slider
	mCtrlDrawDistance = getChild<LLSliderCtrl>("DrawDistance");
	mDrawDistanceMeterText1 = getChild<LLTextBox>("DrawDistanceMeterText1");
	mDrawDistanceMeterText2 = getChild<LLTextBox>("DrawDistanceMeterText2");
	mCtrlDrawDistance->setCommitCallback(updateMeterText);
	mCtrlDrawDistance->setCallbackUserData(this);
	updateMeterText(mCtrlDrawDistance, (void*)this);

	// Object detail slider
	mCtrlLODFactor = getChild<LLSliderCtrl>("ObjectMeshDetail");
	mLODFactorText = getChild<LLTextBox>("ObjectMeshDetailText");
	mCtrlLODFactor->setCommitCallback(updateSliderText);
	mCtrlLODFactor->setCallbackUserData(mLODFactorText);

	// Flex object detail slider
	mCtrlFlexFactor = getChild<LLSliderCtrl>("FlexibleMeshDetail");
	mFlexFactorText = getChild<LLTextBox>("FlexibleMeshDetailText");
	mCtrlFlexFactor->setCommitCallback(updateSliderText);
	mCtrlFlexFactor->setCallbackUserData(mFlexFactorText);

	// Tree detail slider
	mCtrlTreeFactor = getChild<LLSliderCtrl>("TreeMeshDetail");
	mTreeFactorText = getChild<LLTextBox>("TreeMeshDetailText");
	mCtrlTreeFactor->setCommitCallback(updateSliderText);
	mCtrlTreeFactor->setCallbackUserData(mTreeFactorText);

	// Avatar detail slider
	mCtrlAvatarFactor = getChild<LLSliderCtrl>("AvatarMeshDetail");
	mAvatarFactorText = getChild<LLTextBox>("AvatarMeshDetailText");
	mCtrlAvatarFactor->setCommitCallback(updateSliderText);
	mCtrlAvatarFactor->setCallbackUserData(mAvatarFactorText);

	// Terrain detail slider
	mCtrlTerrainFactor = getChild<LLSliderCtrl>("TerrainMeshDetail");
	mTerrainFactorText = getChild<LLTextBox>("TerrainMeshDetailText");
	mCtrlTerrainFactor->setCommitCallback(updateSliderText);
	mCtrlTerrainFactor->setCallbackUserData(mTerrainFactorText);

	// Sky detail slider
	mCtrlSkyFactor = getChild<LLSliderCtrl>("SkyMeshDetail");
	mSkyFactorText = getChild<LLTextBox>("SkyMeshDetailText");
	mCtrlSkyFactor->setCommitCallback(updateSliderText);
	mCtrlSkyFactor->setCallbackUserData(mSkyFactorText);

	// Classic clouds
	mCtrlClassicClouds = getChild<LLCheckBoxCtrl>("ClassicClouds");
	mCtrlClassicClouds->setCommitCallback(onClassicClouds);
	mCtrlClassicClouds->setCallbackUserData(this);
	mClassicCloudsText = getChild<LLTextBox>("ClassicCloudsText");
	mSpinCloudsAltitude = getChild<LLSpinCtrl>("CloudsAltitude");
	LLControlVariable* controlp =
		gSavedSettings.getControl("ClassicCloudsMaxAlt");
	if (!controlp)
	{
		llerrs << "ClassicCloudsMaxAlt debug setting is missing !" << llendl;
	}
	mCommitSignal =
		controlp->getSignal()->connect(boost::bind(&LLPrefsGraphicsImpl::setCloudsMaxAlt,
												   this));
	setCloudsMaxAlt();

	// Particle detail slider
	mCtrlMaxParticle = getChild<LLSliderCtrl>("MaxParticleCount");
	// Local lights count slider
	mCtrlLocalLightCount = getChild<LLSliderCtrl>("LocalLightCount");

	// GPU/GL features sub-tab:
	mCtrlOcclusion = getChild<LLCheckBoxCtrl>("occlusion");
	childSetCommitCallback("texture_compression", onCommitNeedsRefresh, this);
	childSetVisible("after_restart", LLStartUp::isLoggedIn());
#if LL_DARWIN
	// Under macOS, this setting allows to choose between the legacy GL v2.1
	// compatibility profile, and the GL v3.2 (or v4.1, depending on the macOS
	// version) core profile, so this control must always be available, even
	// when running under the v2.1 compatibility profile. HB
	childSetEnabled("core_gl", true);
	childSetToolTip("core_gl", getString("core_gl_macos_tip"));
#else
	childSetEnabled("core_gl", gGLManager.mGLVersion >= 3.f);
#endif
	// Intel iGPUs do not have the necessary GL call for VRAM checks, since
	// they do not have VRAM at all !  This might change with the future ARC
	// discrete GPUs... HB
	if (!gGLManager.mHasATIMemInfo && !gGLManager.mHasNVXMemInfo)
	{
		childSetVisible("no_vram_check", false);
	}

	// Avatars rendering sub-tab:
	std::string off_text = getString("off_text");

	mCtrlMaxNonImpostors = getChild<LLSliderCtrl>("MaxNonImpostors");
	mCtrlMaxNonImpostors->setOffLimit(off_text, 0.f);
	mCtrlMaxNonImpostors->setCommitCallback(onCommitMaxNonImpostors);
	mCtrlMaxNonImpostors->setCallbackUserData(this);

	mCtrlMaximumComplexity = getChild<LLSliderCtrl>("MaximumComplexity");
	mCtrlMaximumComplexity->setOffLimit(off_text, 0.f);

	mCtrlSurfaceAreaLimit = getChild<LLSliderCtrl>("SurfaceAreaLimit");
	mCtrlSurfaceAreaLimit->setOffLimit(off_text, 0.f);

	mCtrlGeometryBytesLimit = getChild<LLSliderCtrl>("GeometryBytesLimit");
	mCtrlGeometryBytesLimit->setOffLimit(off_text, 0.f);

	LLSliderCtrl* slider = getChild<LLSliderCtrl>("MaxPuppetAvatars");
	slider->setOffLimit(off_text, 0.f);

	childSetCommitCallback("AvatarPhysics", onCommitAvatarPhysics, this);

	bool show_rgba16 = gGLManager.mIsAMD && gGLManager.mGLVersion >= 4.f;
	childSetVisible("rgba16_text", show_rgba16);
	childSetVisible("rgba16_check", show_rgba16);

	refresh();

	return true;
}

void LLPrefsGraphicsImpl::initWindowSizeControls()
{
	// Window size
	mWindowSizeLabel = getChild<LLTextBox>("WindowSizeLabel");
	mCtrlWindowSize = getChild<LLComboBox>("windowsize combo");

	// Look to see if current window size matches existing window sizes, if so
	// then just set the selection value...
	const U32 height = gViewerWindowp->getWindowDisplayHeight();
	const U32 width = gViewerWindowp->getWindowDisplayWidth();
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); ++i)
	{
		U32 height_test = 0;
		U32 width_test = 0;
		mCtrlWindowSize->setCurrentByIndex(i);
		if (extractSizeFromString(mCtrlWindowSize->getValue().asString(),
								  width_test, height_test))
		{
			if (height_test == height && width_test == width)
			{
				return;
			}
		}
	}
	// ...otherwise, add a new entry with the current window height/width.
	LLUIString resolution_label = getString("resolution_format");
	resolution_label.setArg("[RES_X]", llformat("%d", width));
	resolution_label.setArg("[RES_Y]", llformat("%d", height));
	mCtrlWindowSize->add(resolution_label, ADD_TOP);
	mCtrlWindowSize->setCurrentByIndex(0);
}

LLPrefsGraphicsImpl::~LLPrefsGraphicsImpl()
{
	mCommitSignal.disconnect();

#if 0
	// Clean up user data
	for (S32 i = 0; i < mCtrlAspectRatio->getItemCount(); ++i)
	{
		mCtrlAspectRatio->setCurrentByIndex(i);
	}
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); ++i)
	{
		mCtrlWindowSize->setCurrentByIndex(i);
	}
#endif
}

void LLPrefsGraphicsImpl::setCloudsMaxAlt()
{
	F32 max_alt = gSavedSettings.getU32("ClassicCloudsMaxAlt");
	mSpinCloudsAltitude->setMinValue(-max_alt);
	mSpinCloudsAltitude->setMaxValue(max_alt);
}

void LLPrefsGraphicsImpl::draw()
{
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastGraphicsPrefTab"));
	}
	if (sNeedsRefresh)
	{
		sNeedsRefresh = false;
		refresh();
	}
	LLPanel::draw();
}

void LLPrefsGraphicsImpl::refresh()
{
	mFullScreen = gSavedSettings.getBool("FullScreen");

	mFSAutoDetectAspect =
		gSavedSettings.getBool("FullScreenAutoDetectAspectRatio");
#if !LL_LINUX	// Irrelevant setting for Linux
	mRenderHiDPI = gSavedSettings.getBool("RenderHiDPI");
#endif

	mQualityPerformance = gSavedSettings.getU32("RenderQualityPerformance");

	F32 bw = gFeatureManager.getGPUMemoryBandwidth();
	if (bw > 0.f)
	{
		mCtrlBenchmark->setToolTip(getString("tool_tip_bench"));
		mCtrlBenchmark->setToolTipArg("[BW]", llformat("%d", (S32)bw));
	}

	// Shaders settings
	mWaterReflectionType = gSavedSettings.getU32("RenderWaterReflectionType");
	mWaterMaxAltitude = gSavedSettings.getU32("RenderWaterMaxAltitude");
	mRenderTransparentWater = gSavedSettings.getBool("RenderTransparentWater");
	mScreenSpaceReflections = gSavedSettings.getBool("RenderScreenSpaceReflections");
	mAvatarCloth = mCanDoCloth && gSavedSettings.getBool("RenderAvatarCloth");
	mUseSMAAShader = gSavedSettings.getBool("RenderDeferredUseSMAA");
	mUseSharpenShader = gSavedSettings.getBool("RenderDeferredAASharpen");

	// Draw distance
	mRenderFarClip = gSavedSettings.getF32("RenderFarClip");

	// Sliders and their text boxes
	mPrimLOD = gSavedSettings.getF32("RenderVolumeLODFactor");
	mMeshLODBoost = gSavedSettings.getF32("MeshLODBoostFactor");
	mFlexLOD = gSavedSettings.getF32("RenderFlexTimeFactor");
	mTreeLOD = gSavedSettings.getF32("RenderTreeLODFactor");
	mAvatarLOD = gSavedSettings.getF32("RenderAvatarLODFactor");
	mTerrainLOD = gSavedSettings.getF32("RenderTerrainLODFactor");
	mSkyLOD = gSavedSettings.getU32("WLSkyDetail");
	mParticleCount = gSavedSettings.getS32("RenderMaxPartCount");
	mPostProcess = gSavedSettings.getU32("RenderGlowResolutionPow");

	// Classic clouds
	mUseClassicClouds = gSavedSettings.getBool("SkyUseClassicClouds");
	mClassicCloudsAvgAlt = gSavedSettings.getS32("ClassicCloudsAvgAlt");

	// Lighting and terrain radios
	mGlow = gSavedSettings.getBool("RenderGlow");
	mGlowStrength = gSavedSettings.getF32("RenderGlowStrength");
	mLocalLightCount = gSavedSettings.getU32("RenderLocalLightCount");
	mExposure = gSavedSettings.getF32("RenderExposure");
	mRenderPBR = gSavedSettings.getBool("RenderUsePBR");
	mRenderDeferred = gSavedSettings.getBool("RenderDeferred");
	mRenderShadowDetail = gSavedSettings.getU32("RenderShadowDetail");
	mReflectionProbeDetail = gSavedSettings.getU32("RenderReflectionProbeDetail");
	mReflectionProbeLevel = gSavedSettings.getU32("RenderReflectionProbeLevel");
	mTerrainDetail = gSavedSettings.getS32("RenderTerrainDetail");

	// Slider text boxes
	updateSliderText(mCtrlLODFactor, mLODFactorText);
	updateSliderText(mCtrlFlexFactor, mFlexFactorText);
	updateSliderText(mCtrlTreeFactor, mTreeFactorText);
	updateSliderText(mCtrlAvatarFactor, mAvatarFactorText);
	updateSliderText(mCtrlTerrainFactor, mTerrainFactorText);
	updateSliderText(mCtrlGlowResolution, mGlowResolutionText);
	updateSliderText(mCtrlSkyFactor, mSkyFactorText);

	// GPU/GL features sub-tab
	mRenderGLCoreProfile = gSavedSettings.getBool("RenderGLCoreProfile");
	mUseAniso = gSavedSettings.getBool("RenderAnisotropic");
	mFSAASamples = gSavedSettings.getU32("RenderFSAASamples");
	mGamma = gSavedSettings.getF32("DisplayGamma");
	mVRAMOverride = gSavedSettings.getU32("VRAMOverride");
	mTextureMem = gSavedSettings.getS32("TextureMemory");
	mMaxBoundTexMem = gSavedSettings.getU32("MaxBoundTexMem");
	mCompressTextures = gGLManager.mGLVersion >= 2.1f &&
						gSavedSettings.getBool("RenderCompressTextures");
	mRenderCompressThreshold =
		gSavedSettings.getU32("RenderCompressThreshold");
	mDisableVRAMCheck = gSavedSettings.getBool("DisableVRAMCheck");
	childSetValue("fsaa", (LLSD::Integer)mFSAASamples);
	mRenderUseRGBA16ATI = gSavedSettings.getBool("RenderUseRGBA16ATI");
	mGLWorkerThreads = gSavedSettings.getS32("GLWorkerThreads");

	// Avatars rendering sub-tab
	mNonImpostors = gSavedSettings.getU32("RenderAvatarMaxNonImpostors");
	mNonImpostorsPuppets = gSavedSettings.getU32("RenderAvatarMaxPuppets");
	mAvatarPhysics = gSavedSettings.getBool("AvatarPhysics");
	mAlwaysRenderFriends = gSavedSettings.getBool("AlwaysRenderFriends");
	mColoredJellyDolls = gSavedSettings.getBool("ColoredJellyDolls");
	mRenderAvatarPhysicsLODFactor =
		gSavedSettings.getF32("RenderAvatarPhysicsLODFactor");
	mRenderAvatarMaxComplexity =
		gSavedSettings.getU32("RenderAvatarMaxComplexity");
	mRenderAutoMuteSurfaceAreaLimit =
		gSavedSettings.getF32("RenderAutoMuteSurfaceAreaLimit");
	mRenderAutoMuteMemoryLimit =
		gSavedSettings.getU32("RenderAutoMuteMemoryLimit");

	refreshEnabledState();
}

void LLPrefsGraphicsImpl::refreshEnabledState()
{
	// Windowed/full-screen modes UI elements visibility
	mDisplayResLabel->setVisible(mFullScreen);
	mCtrlFullScreen->setVisible(mFullScreen);
	mCtrlAspectRatio->setVisible(mFullScreen);
	mAspectRatioLabel1->setVisible(mFullScreen);
	mCtrlAutoDetectAspect->setVisible(mFullScreen);
	mCtrlWindowSize->setVisible(!mFullScreen);
	mFullScreenInfo->setVisible(!mFullScreen);
	mWindowSizeLabel->setVisible(!mFullScreen);

	// Glow
	if (LLPipeline::sCanRenderGlow)
	{
		mCtrlGlow->setEnabled(true);
		bool glow_enabled = mCtrlGlow->get();
		mRenderGlowStrength->setEnabled(glow_enabled);
		mCtrlGlowResolution->setEnabled(glow_enabled);
		mGlowResolutionText->setEnabled(glow_enabled);
	}
	else
	{
		mCtrlGlow->setEnabled(false);
		mRenderGlowStrength->setEnabled(false);
		mCtrlGlowResolution->setEnabled(false);
		mGlowResolutionText->setEnabled(false);
	}

	// Classic clouds
	bool clouds = mCtrlClassicClouds->get();
	mSpinCloudsAltitude->setEnabled(clouds);

	// Avatar Mode
	bool skinning = !LLStartUp::isLoggedIn() ||
					gViewerShaderMgrp->mMaxAvatarShaderLevel > 0;
	bool cloth = mCanDoCloth && skinning;
	// Avatar cloth
	mCtrlAvatarCloth->setEnabled(cloth);
	if (!cloth)
	{
		mCtrlAvatarCloth->setValue(false);
	}

	// Deferred rendering
	bool deferred = mCanDoDeferred && skinning;
	mCtrlPBREnable->setVisible(deferred);
	if (gUsePBRShaders)
	{
		mCtrlDeferredEnable->setVisible(false);
		childSetVisible("water_text", false);
		mComboWaterReflectionType->setVisible(false);
		mCtrlAvatarCloth->setVisible(false);
		mCtrlExposure->setVisible(true);
		mCtrlTransparentWater->setVisible(true);
		mCtrlScreenSpaceReflections->setVisible(true);
		childSetVisible("reflections_text", true);
		mComboReflectionDetail->setVisible(true);
		childSetVisible("coverage_text", true);
		childSetEnabled("coverage_text", LLViewerShaderMgr::sHasRP);
		mComboReflectionLevel->setVisible(true);
		mComboReflectionLevel->setEnabled(LLViewerShaderMgr::sHasRP);
	}
	else
	{
		mCtrlExposure->setVisible(false);
		mCtrlTransparentWater->setVisible(false);
		mCtrlScreenSpaceReflections->setVisible(false);
		childSetVisible("reflections_text", false);
		mComboReflectionDetail->setVisible(false);
		childSetVisible("coverage_text", false);
		mComboReflectionLevel->setVisible(false);
		mCtrlDeferredEnable->setVisible(true);
		childSetVisible("water_text", true);
		mComboWaterReflectionType->setVisible(true);
		mCtrlAvatarCloth->setVisible(true);
		mCtrlPBREnable->setEnabled(gGLManager.mGLVersion >= 3.1f);
		mCtrlDeferredEnable->setEnabled(deferred);
		if (!deferred)
		{
			mCtrlDeferredEnable->setValue(false);
		}
	}
	bool alm_on = gUsePBRShaders || mCtrlDeferredEnable->get();
	mComboRenderShadowDetail->setEnabled(alm_on);
	// Visibility of settings depending on ALM shaders
#if HB_PBR_SMAA_AND_CAS
	bool can_do_smaa_or_cas = alm_on;
#else
	bool can_do_smaa_or_cas = alm_on && !gUsePBRShaders;
#endif
	mCtrlSMAA->setVisible(can_do_smaa_or_cas && LLViewerShaderMgr::sHasSMAA);
	mCtrlSharpen->setVisible(can_do_smaa_or_cas && LLViewerShaderMgr::sHasCAS);

	deferred &= gUsePBRShaders || mCtrlDeferredEnable->get();
	mComboRenderShadowDetail->setVisible(deferred);
	childSetVisible("no_alm_text", !deferred);
	childSetToolTip("no_alm_text", getString("tool_tip_no_deferred"));

	// GPU/GL features sub-tab
	S32 min_tex_mem = LLViewerTextureList::getMinVideoRamSetting();
	S32 max_tex_mem = LLViewerTextureList::getMaxVideoRamSetting(true);
	childSetMinValue("GrapicsCardTextureMemory", min_tex_mem);
	childSetMaxValue("GrapicsCardTextureMemory", max_tex_mem);

	if (!gFeatureManager.isFeatureAvailable("RenderCompressTextures") ||
		gGLManager.mGLVersion < 2.1f)
	{
		childSetEnabled("texture_compression", false);
	}

	if (!gFeatureManager.isFeatureAvailable("UseOcclusion"))
	{
		mCtrlOcclusion->setEnabled(false);
	}

	// Texture compression settings.
	bool compress = gSavedSettings.getBool("RenderCompressTextures");
	childSetEnabled("compress_throttle", compress);
	childSetEnabled("pixels_text", compress);

	// Avatars rendering sub-tab
	bool impostors = mCtrlMaxNonImpostors->getValue().asInteger() > 0;
	mCtrlMaximumComplexity->setEnabled(impostors);
	mCtrlSurfaceAreaLimit->setEnabled(impostors);
	mCtrlGeometryBytesLimit->setEnabled(impostors);
	childSetEnabled("AvatarPhysicsLOD",
					gSavedSettings.getBool("AvatarPhysics"));
}

void LLPrefsGraphicsImpl::cancel()
{
	gSavedSettings.setBool("FullScreen", mFullScreen);
	gSavedSettings.setBool("FullScreenAutoDetectAspectRatio",
						   mFSAutoDetectAspect);
#if !LL_LINUX	// Irrelevant setting for Linux
	gSavedSettings.setBool("RenderHiDPI", mRenderHiDPI);
#endif
	gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);

	gSavedSettings.setU32("RenderQualityPerformance", mQualityPerformance);

	gSavedSettings.setU32("RenderWaterReflectionType", mWaterReflectionType);
	gSavedSettings.setU32("RenderWaterMaxAltitude", mWaterMaxAltitude);
	gSavedSettings.setBool("RenderTransparentWater", mRenderTransparentWater);
	gSavedSettings.setBool("RenderScreenSpaceReflections", mScreenSpaceReflections);
	gSavedSettings.setBool("RenderAvatarCloth", mAvatarCloth);

	gSavedSettings.setBool("SkyUseClassicClouds", mUseClassicClouds);
	gSavedSettings.setS32("ClassicCloudsAvgAlt", mClassicCloudsAvgAlt);

	gSavedSettings.setBool("RenderUsePBR", mRenderPBR);
	gSavedSettings.setBool("RenderDeferred", mRenderDeferred);
	gSavedSettings.setU32("RenderShadowDetail", mRenderShadowDetail);
	gSavedSettings.setBool("RenderGlow", mGlow);
	gSavedSettings.setF32("RenderGlowStrength", mGlowStrength);
	gSavedSettings.setU32("RenderLocalLightCount", mLocalLightCount);
	gSavedSettings.setF32("RenderExposure", mExposure);
	gSavedSettings.setU32("RenderReflectionProbeDetail", mReflectionProbeDetail);
	gSavedSettings.setU32("RenderReflectionProbeLevel", mReflectionProbeLevel);
	gSavedSettings.setBool("RenderDeferredUseSMAA", mUseSMAAShader);
	gSavedSettings.setBool("RenderDeferredAASharpen", mUseSharpenShader);

	gSavedSettings.setS32("RenderTerrainDetail", mTerrainDetail);

	gSavedSettings.setF32("RenderFarClip", mRenderFarClip);
	gSavedSettings.setF32("RenderVolumeLODFactor", mPrimLOD);
	gSavedSettings.setF32("MeshLODBoostFactor", mMeshLODBoost);
	gSavedSettings.setF32("RenderFlexTimeFactor", mFlexLOD);
	gSavedSettings.setF32("RenderTreeLODFactor", mTreeLOD);
	gSavedSettings.setF32("RenderAvatarLODFactor", mAvatarLOD);
	gSavedSettings.setF32("RenderTerrainLODFactor", mTerrainLOD);
	gSavedSettings.setU32("WLSkyDetail", mSkyLOD);
	gSavedSettings.setS32("RenderMaxPartCount", mParticleCount);
	gSavedSettings.setU32("RenderGlowResolutionPow", mPostProcess);

	// GPU/GL features sub-tab
	gSavedSettings.setBool("RenderGLCoreProfile", mRenderGLCoreProfile);
	gSavedSettings.setBool("RenderAnisotropic", mUseAniso);
	gSavedSettings.setU32("RenderFSAASamples", mFSAASamples);
	gSavedSettings.setF32("DisplayGamma", mGamma);
	gSavedSettings.setU32("VRAMOverride", mVRAMOverride);
	gSavedSettings.setS32("TextureMemory", mTextureMem);
	gSavedSettings.setU32("MaxBoundTexMem", mMaxBoundTexMem);
	gSavedSettings.setBool("RenderCompressTextures", mCompressTextures);
	gSavedSettings.setU32("RenderCompressThreshold", mRenderCompressThreshold);
	gSavedSettings.setBool("DisableVRAMCheck", mDisableVRAMCheck);
	gSavedSettings.setBool("RenderUseRGBA16ATI", mRenderUseRGBA16ATI);
	gSavedSettings.setS32("GLWorkerThreads", mGLWorkerThreads);

	// Avatars rendering sub-tab
	gSavedSettings.setU32("RenderAvatarMaxNonImpostors", mNonImpostors);
	gSavedSettings.setU32("RenderAvatarMaxPuppets", mNonImpostorsPuppets);
	gSavedSettings.setBool("AvatarPhysics", mAvatarPhysics);
	gSavedSettings.setBool("AlwaysRenderFriends", mAlwaysRenderFriends);
	gSavedSettings.setBool("ColoredJellyDolls", mColoredJellyDolls);
	gSavedSettings.setF32("RenderAvatarPhysicsLODFactor",
						  mRenderAvatarPhysicsLODFactor);
	gSavedSettings.setU32("RenderAvatarMaxComplexity",
						  mRenderAvatarMaxComplexity);
	gSavedSettings.setF32("RenderAutoMuteSurfaceAreaLimit",
						  mRenderAutoMuteSurfaceAreaLimit);
	gSavedSettings.setU32("RenderAutoMuteMemoryLimit",
						  mRenderAutoMuteMemoryLimit);
}

void LLPrefsGraphicsImpl::apply()
{
	applyResolution();
	applyWindowSize();
}

void LLPrefsGraphicsImpl::applyResolution()
{
	gGL.flush();
	glFinish();

	bool restart_display = false;
	bool after_restart = false;

	bool full_screen = gWindowp->getFullscreen();
	bool want_full_screen = !mCtrlWindowed->get();

	char aspect_ratio_text[ASPECT_RATIO_STR_LEN];
	if (mCtrlAspectRatio->getCurrentIndex() == -1)
	{
		// Cannot pass const char* from c_str() into strtok
		strncpy(aspect_ratio_text, mCtrlAspectRatio->getSimple().c_str(),
				sizeof(aspect_ratio_text) -1);
		aspect_ratio_text[sizeof(aspect_ratio_text) -1] = '\0';
		char* element = strtok(aspect_ratio_text, ":/\\");
		if (!element)
		{
			mAspectRatio = 0.f; // Will be clamped later
		}
		else
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			mAspectRatio = (F32)atof(element);
		}

		// Look for denominator
		element = strtok(NULL, ":/\\");
		if (element)
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			F32 denominator = (F32)atof(element);
			if (denominator > 0.f)
			{
				mAspectRatio /= denominator;
			}
		}
	}
	else
	{
		mAspectRatio = (F32)mCtrlAspectRatio->getValue().asReal();
	}
	// Presumably, user entered a non-numeric value if mAspectRatio == 0.f
	if (mAspectRatio != 0.f)
	{
		mAspectRatio = llclamp(mAspectRatio, 0.2f, 5.f);
		gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);
	}

	// Screen resolution
	S32 num_resolutions;
	LLWindow::LLWindowResolution* resolutions =
		gWindowp->getSupportedResolutions(num_resolutions);
	S32 res_idx = mCtrlFullScreen->getCurrentIndex();
	if (res_idx >= 0 && res_idx < num_resolutions)
	{
		S32 width = resolutions[res_idx].mWidth;
		S32 height = resolutions[res_idx].mHeight;
		S32 old_width = gSavedSettings.getS32("FullScreenWidth");
		S32 old_height = gSavedSettings.getS32("FullScreenHeight");
		if (old_width != width)
		{
			gSavedSettings.setS32("FullScreenWidth", width);
			if (want_full_screen && full_screen)
			{
				after_restart = true;
			}
		}
		if (old_height != height)
		{
			gSavedSettings.setS32("FullScreenHeight", height);
			if (want_full_screen && full_screen)
			{
				after_restart = true;
			}
		}
	}

	gViewerWindowp->requestResolutionUpdate();

	send_agent_update(true);

	// GPU/GL features sub-tab
	if (gSavedSettings.getBool("RenderGLCoreProfile") != mRenderGLCoreProfile)
	{
		after_restart = true;
	}

	if (gSavedSettings.getU32("RenderFSAASamples") != mFSAASamples)
	{
		after_restart = true;
	}

	if (gSavedSettings.getBool("RenderAnisotropic") != mUseAniso)
	{
		restart_display = true;
	}

	if (gSavedSettings.getS32("GLWorkerThreads") != mGLWorkerThreads)
	{
		restart_display = true;
	}

#if !LL_LINUX	// Irrelevant setting for Linux
	if (gSavedSettings.getBool("RenderHiDPI") != mRenderHiDPI)
	{
		after_restart = true;
	}
#endif

	// We do not support any more full screen <--> windowed mode changes during
	// sessions (and when in full screen mode, we do start it before displaying
	// the login screen), since those have always been prone to failures, black
	// screens and crashes. HB
	if (want_full_screen != full_screen)
	{
		after_restart = true;
	}

	// There are currently issues with core GL profile and display settings
	// changes, so require a restart instead for those. HB
	if (restart_display && LLRender::sGLCoreProfile)
	{
		restart_display = false;
		after_restart = true;
	}

	if (restart_display)
	{
		gViewerWindowp->restartDisplay();
	}

	if (after_restart)
	{
		gNotifications.add("InEffectAfterRestart");
	}

	// Update enable/disable
	refresh();
}

// Extract from strings of the form "<width> x <height>", e.g. "640 x 480".
bool LLPrefsGraphicsImpl::extractSizeFromString(const std::string& instr,
												U32& width, U32& height)
{
	try
	{
		std::cmatch what;
		const std::regex expression("([0-9]+) x ([0-9]+)");
		if (std::regex_match(instr.c_str(), what, expression))
		{
			width = atoi(what[1].first);
			height = atoi(what[2].first);
			return true;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	width = height = 0;
	return false;
}

void LLPrefsGraphicsImpl::applyWindowSize()
{
	// Only apply the new window size in real time (i.e. without a restart)
	// when in windowed mode and when the user wants to change the size for
	// that mode. Changing the size (i.e. the resolution or scaling) while in
	// full screen mode most often fails with a black screen or worst, and if
	// the user did not ask for a size change for the windowed mode while we
	// are running in this mode, then we do not care. HB
	if (!(bool)mCtrlWindowed->get() || gWindowp->getFullscreen() ||
		// Check for bogus index (missing combo ?)
		mCtrlWindowSize->getCurrentIndex() == -1)
	{
		return;
	}
	U32 width = 0;
	U32 height = 0;
	std::string res_str = mCtrlWindowSize->getValue().asString();
	if (extractSizeFromString(res_str, width, height))
	{
		gViewerWindowp->resizeWindow(width, height);
	}
}

void LLPrefsGraphicsImpl::onChangeQuality(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLSliderCtrl* sldr = (LLSliderCtrl*)ctrl;
	if (self && sldr)
	{
		U32 set = (U32)sldr->getValueF32();
		gFeatureManager.setGraphicsLevel(set, true);

		self->refreshEnabledState();
		self->refresh();
		self->applyResolution();
	}
}

//static
void LLPrefsGraphicsImpl::onTabChanged(void* data, bool)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastGraphicsPrefTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void LLPrefsGraphicsImpl::onOpenHelp(void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (!self) return;

	LLFloater* parentp = gFloaterViewp->getParentFloater(self);
	if (!parentp) return;

	gNotifications.add(parentp->contextualNotification("GraphicsPreferencesHelp"));
}

//static
void LLPrefsGraphicsImpl::onCommitWindowedMode(LLUICtrl*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		// Store the mode the user wants.
		gSavedSettings.setBool("FullScreen", !self->mCtrlWindowed->get());
		self->refresh();
	}
}

//static
void LLPrefsGraphicsImpl::onCommitAutoDetectAspect(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (!self || !ctrl) return;

	bool auto_detect = ((LLCheckBoxCtrl*)ctrl)->get();
	if (auto_detect)
	{
		S32 numerator = 0;
		S32 denominator = 0;

		// Clear any aspect ratio override
		gWindowp->setNativeAspectRatio(0.f);
		fractionFromDecimal(gWindowp->getNativeAspectRatio(), numerator,
							denominator);
		std::string aspect;
		if (numerator != 0)
		{
			aspect = llformat("%d:%d", numerator, denominator);
		}
		else
		{
			aspect = llformat("%.3f", gWindowp->getNativeAspectRatio());
		}

		self->mCtrlAspectRatio->setLabel(aspect);

		F32 ratio = gWindowp->getNativeAspectRatio();
		gSavedSettings.setF32("FullScreenAspectRatio", ratio);
	}
}

//static
void LLPrefsGraphicsImpl::onKeystrokeAspectRatio(LLLineEditor*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->mCtrlAutoDetectAspect->set(false);
	}
}

//static
void LLPrefsGraphicsImpl::onSelectAspectRatio(LLUICtrl*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->mCtrlAutoDetectAspect->set(false);
	}
}

//static
void LLPrefsGraphicsImpl::fractionFromDecimal(F32 decimal_val, S32& numerator,
										 S32& denominator)
{
	numerator = 0;
	denominator = 0;
	for (F32 test_denominator = 1.f; test_denominator < 30.f;
		 test_denominator += 1.f)
	{
		if (fmodf((decimal_val * test_denominator) + 0.01f, 1.f) < 0.02f)
		{
			numerator = ll_round(decimal_val * test_denominator);
			denominator = ll_round(test_denominator);
			break;
		}
	}
}

//static
void LLPrefsGraphicsImpl::onCommitNeedsRefresh(LLUICtrl*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->refreshEnabledState();
	}
}

//static
void LLPrefsGraphicsImpl::onCommitAvatarPhysics(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->childSetEnabled("AvatarPhysicsLOD", enabled);
}

//static
void LLPrefsGraphicsImpl::onCommitMaxNonImpostors(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!self || !slider) return;

	bool enabled = slider->getValue().asInteger() > 0;
	self->mCtrlMaximumComplexity->setEnabled(enabled);
	self->mCtrlSurfaceAreaLimit->setEnabled(enabled);
	self->mCtrlGeometryBytesLimit->setEnabled(enabled);
}

void LLPrefsGraphicsImpl::setHardwareDefaults(void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		gFeatureManager.applyRecommendedSettings();
		self->refreshEnabledState();
		self->refresh();
	}
}

void LLPrefsGraphicsImpl::updateSliderText(LLUICtrl* ctrl, void* data)
{
	// Get our UI widgets
	LLTextBox* text_box = (LLTextBox*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!text_box || !slider)
	{
		return;
	}

	// Get range and points when text should change
	F32 range = slider->getMaxValue() - slider->getMinValue();
	llassert(range > 0);
	F32 midPoint = slider->getMinValue() + range / 3.f;
	F32 highPoint = slider->getMinValue() + (2.f / 3.f) * range;

	// Choose the right text
	if (slider->getValueF32() < midPoint)
	{
		text_box->setText("Low");
	}
	else if (slider->getValueF32() < highPoint)
	{
		text_box->setText("Mid");
	}
	else
	{
		text_box->setText("High");
	}
}

void LLPrefsGraphicsImpl::updateMeterText(LLUICtrl* ctrl, void* data)
{
	// Get our UI widgets
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (self && ctrl)
	{
		// Toggle the two text boxes based on whether we have 1 or two digits
		F32 val = slider->getValueF32();
		bool two_digits = val < 100;
		self->mDrawDistanceMeterText1->setVisible(two_digits);
		self->mDrawDistanceMeterText2->setVisible(!two_digits);
	}
}

void LLPrefsGraphicsImpl::onClassicClouds(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->mSpinCloudsAltitude->setEnabled(check->get());
	}
}

//-----------------------------------------------------------------------------

LLPrefsGraphics::LLPrefsGraphics()
:	impl(*new LLPrefsGraphicsImpl())
{
}

LLPrefsGraphics::~LLPrefsGraphics()
{
	delete &impl;
}

void LLPrefsGraphics::apply()
{
	impl.apply();
}

void LLPrefsGraphics::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsGraphics::getPanel()
{
	return &impl;
}

//static
void LLPrefsGraphics::refresh()
{
	sNeedsRefresh = true;
}
