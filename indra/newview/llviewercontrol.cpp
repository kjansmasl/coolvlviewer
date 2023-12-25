/**
 * @file llviewercontrol.cpp
 * @brief Viewer configuration
 * @author Richard Nelson
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

#include "llviewercontrol.h"

#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "llconsole.h"
#include "llcorehttpcommon.h"
#include "llerrorcontrol.h"
#include "llfloater.h"
#include "llgl.h"
#include "llimagegl.h"
#include "llkeyboard.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llrender.h"
#include "llspellcheck.h"
#include "llsys.h"
#include "llversionviewer.h"
#include "llvolume.h"
#include "llxmlrpctransaction.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "lldebugview.h"
#include "lldrawpoolbump.h"
#include "lldrawpooltree.h"
#include "llenvironment.h"
#include "llfasttimerview.h"
#include "llfeaturemanager.h"
#include "llflexibleobject.h"
#include "hbfloaterdebugtags.h"
#include "hbfloatereditenvsettings.h"
#include "hbfloatersearch.h"
#include "llfloaterstats.h"
#include "llfloaterwindlight.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llgroupnotify.h"
#include "llhudeffectlookat.h"
#include "llinventorymodelfetch.h"
#include "llmeshrepository.h"
#include "llpanelminimap.h"
#include "llpipeline.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpuppetmotion.h"
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llsky.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "llsurfacepatch.h"
#include "lltoolbar.h"
#include "lltracker.h"
#include "llvieweraudio.h"
#include "hbviewerautomation.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvoiceclient.h"
#include "llvosky.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowlsky.h"
#include "llwlskyparammgr.h"
#include "llworld.h"

std::map<std::string, LLControlGroup*> gSettings;
// Those two are saved at end of the session
LLControlGroup gSavedSettings("Global");
LLControlGroup gSavedPerAccountSettings("PerAccount");
// Read-only
LLControlGroup gColors("Colors");

extern bool gUpdateDrawDistance;

////////////////////////////////////////////////////////////////////////////
// Listeners

static bool handleAutoReloadFailedPatchTexDelayChanged(const LLSD& newvalue)
{
	LLSurfacePatch::setAutoReloadDelay(newvalue.asInteger());
	return true;
}

static bool handleDebugPermissionsChanged(const LLSD& newvalue)
{
	dialog_refresh_all();
	return true;
}

static bool handleHighResSnapshotChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		// High Res Snapshot active, must uncheck RenderUIInSnapshot
		gSavedSettings.setBool("RenderUIInSnapshot", false);
	}
	return true;
}

#if LL_FAST_TIMERS_ENABLED
static bool handleFastTimersAlwaysEnabledChanged(const LLSD& newvalue)
{
	if (gFastTimerViewp && gFastTimerViewp->getVisible())
	{
		// Nothing to do
		return true;
	}
	if (newvalue.asBoolean())
	{
		gEnableFastTimers = true;
		llinfos << "Fast timers enabled." << llendl;
	}
	else
	{
		gEnableFastTimers = false;
		llinfos << "Fast timers disabled." << llendl;
	}
	return true;
}
#endif	// LL_FAST_TIMERS_ENABLED

static bool handleRenderCompressTexturesChanged(const LLSD& newvalue)
{
	if (gFeatureManager.isFeatureAvailable("RenderCompressTextures"))
	{
		LLImageGL::sCompressTextures =
			gGLManager.mGLVersion >= 2.1f &&
			gSavedSettings.getBool("RenderCompressTextures");
		LLImageGL::sCompressThreshold =
			gSavedSettings.getU32("RenderCompressThreshold");
	}
	return true;
}

static bool handleRenderFarClipChanged(const LLSD& newvalue)
{
	gUpdateDrawDistance = true;	// Updated in llviewerdisplay.cpp
	return true;
}

static bool handleSetShaderChanged(const LLSD& newvalue)
{
	// Changing shader level may invalidate existing cached bump maps, as the
	// shader type determines the format of the bump map it expects - clear
	// and repopulate the bump cache
	gBumpImageList.destroyGL();
	gBumpImageList.restoreGL();
	LLPipeline::refreshCachedSettings();
	gViewerShaderMgrp->setShaders();
	return true;
}

static bool handleRenderDeferredChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		LLPipeline::refreshCachedSettings();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		gViewerShaderMgrp->setShaders();
		// Rebuild objects to make sure all will properly show up... HB
		handle_objects_visibility(NULL);
	}
	return true;
}

static bool handleReflectionProbesChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		LLPipeline::refreshCachedSettings();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		gViewerShaderMgrp->setShaders();
		gPipeline.mReflectionMapManager.reset();
	}
	return true;
}

static bool handleAvatarPhysicsChanged(const LLSD& newvalue)
{
	LLVOAvatar::sAvatarPhysics = newvalue.asBoolean();
	gAgent.sendAgentSetAppearance();
	return true;
}

static bool handleBakeOnMeshUploadsChanged(const LLSD& newvalue)
{
	gAgent.setUploadedBakesLimit();
	return true;
}

static bool handlePuppetryAllowedChanged(const LLSD& newvalue)
{
	LLPuppetMotion::updatePuppetryEnabling();
	return true;
}

static bool handleRenderWaterReflectionTypeChanged(const LLSD& newvalue)
{
	LLPipeline::refreshCachedSettings();
	gWorld.updateWaterObjects();
	return true;
}

static bool handleMeshMaxConcurrentRequestsChanged(const LLSD& newvalue)
{
	LLMeshRepoThread::sMaxConcurrentRequests = (U32)newvalue.asInteger();
	return true;
}

static bool handleShadowsResized(const LLSD& newvalue)
{
	gResizeShadowTexture = true;
	return true;
}

static bool handleRenderGLImageSyncInThread(const LLSD& newvalue)
{
	LLImageGL::sSyncInThread = newvalue.asBoolean();
	return true;
}

static bool handleGLBufferChanged(const LLSD& newvalue)
{
	LLPipeline::refreshCachedSettings();
	if (gPipeline.isInit())
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
	}
	return true;
}

static bool handleLUTBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseLUTBuffers();
		gPipeline.createLUTBuffers();
	}
	return true;
}

static bool handleVolumeSettingsChanged(const LLSD& newvalue)
{
	LLVOVolume::updateSettings();
	return true;
}

static bool handleSkyUseClassicCloudsChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean())
	{
		gWorld.killClouds();
	}
	return true;
}

static bool handleTerrainLODChanged(const LLSD& newvalue)
{
	LLVOSurfacePatch::sLODFactor = (F32)newvalue.asReal();
	// Square lod factor to get exponential range of [0, 4] and keep a value of
	// 1 in the middle of the detail slider for consistency with other detail
	// sliders (see panel_preferences_graphics1.xml)
	LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor;
	return true;
}

static bool handleTreeSettingsChanged(const LLSD& newvalue)
{
	LLVOTree::updateSettings();
	return true;
}

static bool handleFlexLODChanged(const LLSD& newvalue)
{
	LLVolumeImplFlexible::sUpdateFactor = (F32)newvalue.asReal();
	return true;
}

static bool handleGammaChanged(const LLSD& newvalue)
{
	F32 gamma = (F32)newvalue.asReal();
	if (gWindowp && gamma != gWindowp->getGamma())
	{
		// Only save it if it changed
		if (!gWindowp->setGamma(gamma))
		{
			llwarns << "Failed to set the display gamma to " << gamma
					<< ". Restoring the default gamma." << llendl;
			gWindowp->restoreGamma();
		}
	}

	return true;
}

static bool handleMaxPartCountChanged(const LLSD& newvalue)
{
	LLViewerPartSim::setMaxPartCount(newvalue.asInteger());
	return true;
}

static bool handleVideoMemoryChanged(const LLSD& newvalue)
{
	// Note: not using newvalue.asInteger() because this callback is also
	// used after updating MaxBoundTexMem. HB
	gTextureList.updateMaxResidentTexMem(gSavedSettings.getS32("TextureMemory"));
	return true;
}

static bool handleBandwidthChanged(const LLSD& newvalue)
{
	gViewerThrottle.setMaxBandwidth(newvalue.asInteger());
	return true;
}

static bool handleDebugConsoleMaxLinesChanged(const LLSD& newvalue)
{
	if (gDebugViewp && gDebugViewp->mDebugConsolep)
	{
		gDebugViewp->mDebugConsolep->setMaxLines(newvalue.asInteger());
	}
	return true;
}

static bool handleChatConsoleMaxLinesChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setMaxLines(newvalue.asInteger());
	}
	return true;
}

static bool handleChatFontSizeChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setFontSize(newvalue.asInteger());
	}
	return true;
}

static bool handleChatPersistTimeChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setLinePersistTime((F32) newvalue.asReal());
	}
	return true;
}

static bool handleAudioVolumeChanged(const LLSD& newvalue)
{
	audio_update_volume(true);
	return true;
}

static bool handleStackMinimizedTopToBottom(const LLSD& newvalue)
{
	LLFloaterView::setStackMinimizedTopToBottom(newvalue.asBoolean());
	return true;
}

static bool handleStackMinimizedRightToLeft(const LLSD& newvalue)
{
	LLFloaterView::setStackMinimizedRightToLeft(newvalue.asBoolean());
	return true;
}

static bool handleStackScreenWidthFraction(const LLSD& newvalue)
{
	LLFloaterView::setStackScreenWidthFraction(newvalue.asInteger());
	return true;
}

static bool handleJoystickChanged(const LLSD& newvalue)
{
	LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);
	return true;
}

static bool handleAvatarOffsetChanged(const LLSD& newvalue)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->scheduleHoverUpdate();
	}
	return true;
}

static bool handleCameraCollisionsChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gAgent.setCameraCollidePlane(LLVector4(0.f, 0.f, 0.f, 1.f));
	}
	return true;
}

static bool handleCameraChanged(const LLSD& newvalue)
{
	gAgent.setupCameraView();
	return true;
}

static bool handleTrackFocusObjectChanged(const LLSD& newvalue)
{
	gAgent.setObjectTracking(newvalue.asBoolean());
	return true;
}

static bool handlePrimMediaChanged(const LLSD& newvalue)
{
	LLVOVolume::initSharedMedia();
	return true;
}

static bool handleAudioStreamMusicChanged(const LLSD& newvalue)
{
	if (gAudiop)
	{
		if (newvalue.asBoolean())
		{
			LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
			if (parcel && !parcel->getMusicURL().empty())
			{
				// If stream is already playing, do not call this otherwise
				// music will briefly stop
				if (!gAudiop->isInternetStreamPlaying())
				{
					LLViewerParcelMedia::playStreamingMusic(parcel);
				}
			}
		}
		else
		{
			gAudiop->stopInternetStream();
		}
	}
	return true;
}

static bool handleUseOcclusionChanged(const LLSD& newvalue)
{
	LLPipeline::sUseOcclusion =
		newvalue.asBoolean() && !gUseWireframe &&
		gFeatureManager.isFeatureAvailable("UseOcclusion") ? 2 : 0;
	return true;
}

static bool handleNumpadControlChanged(const LLSD& newvalue)
{
	if (gKeyboardp)
	{
		gKeyboardp->setNumpadDistinct((LLKeyboard::e_numpad_distinct)newvalue.asInteger());
	}
	return true;
}

static bool handleWLSkyDetailChanged(const LLSD& newvalue)
{
	LLVOWLSky::updateSettings();
	return true;
}

static bool handleRenderBatchedGlyphsChanged(const LLSD& newvalue)
{
	LLFontGL::setUseBatchedRender((bool)newvalue.asBoolean());
	return true;
}

static bool handleResetVertexBuffersChanged(const LLSD&)
{
	LLVOVolume::sRenderMaxVBOSize = gSavedSettings.getU32("RenderMaxVBOSize");
	if (gPipeline.isInit())
	{
		gPipeline.resetVertexBuffers();
	}
	LLVOTree::updateSettings();

	return true;
}

static bool handleRenderGLUseVBCacheChanged(const LLSD& newvalue)
{
	LLRender::sUseBufferCache = newvalue.asBoolean();
	if (gPipeline.isInit())
	{
		gPipeline.resetVertexBuffers();
	}
	return true;
}

static bool handleRenderOptimizeMeshVertexCacheChanged(const LLSD& newvalue)
{
	LLVolume::sOptimizeCache = newvalue.asBoolean();
	return true;
}

static bool handleNoVerifySSLCertChanged(const LLSD& newvalue)
{
	LLXMLRPCTransaction::setVerifyCert(!newvalue.asBoolean());
	return true;
}

static bool handleEnableHTTP2Changed(const LLSD& newvalue)
{
	LLCore::LLHttp::gEnabledHTTP2 = newvalue.asBoolean();
	return true;
}

static bool handlePingInterpolateChanged(const LLSD& newvalue)
{
	LLViewerObject::setPingInterpolate(newvalue.asBoolean());
	return true;
}

static bool handleVelocityInterpolateChanged(const LLSD& newvalue)
{
	LLViewerObject::setVelocityInterpolate(newvalue.asBoolean());
	return true;
}

static bool handleInterpolationTimesChanged(const LLSD& newvalue)
{
	LLViewerObject::setUpdateInterpolationTimes(gSavedSettings.getF32("InterpolationTime"),
												gSavedSettings.getF32("InterpolationPhaseOut"),
												gSavedSettings.getF32("RegionCrossingInterpolationTime"));
	return true;
}

static bool handleRepartition(const LLSD&)
{
	if (gPipeline.isInit())
	{
		gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
		gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");
		gObjectList.repartitionObjects();
	}
	return true;
}

static bool handleRenderDynamicLODChanged(const LLSD& newvalue)
{
	LLPipeline::sDynamicLOD = newvalue.asBoolean();
	return true;
}

static bool handleAvatarDebugSettingsChanged(const LLSD&)
{
	LLVOAvatar::updateSettings();
	return true;
}

static bool handleDisplayNamesUsageChanged(const LLSD& newvalue)
{
	LLAvatarNameCache::setUseDisplayNames((U32)newvalue.asInteger());
	LLVOAvatar::invalidateNameTags();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleOmitResidentAsLastNameChanged(const LLSD& newvalue)
{
	LLAvatarName::sOmitResidentAsLastName = newvalue.asBoolean();
	LLVOAvatar::invalidateNameTags();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleLegacyNamesForFriendsChanged(const LLSD& newvalue)
{
	LLAvatarName::sLegacyNamesForFriends = newvalue.asBoolean();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleLegacyNamesForSpeakersChanged(const LLSD& newvalue)
{
	LLAvatarName::sLegacyNamesForSpeakers = newvalue.asBoolean();
	return true;
}

static bool handleRenderResolutionDivisorChanged(const LLSD&)
{
	gResizeScreenTexture = true;
	return true;
}

static bool handleDebugObjectIdChanged(const LLSD& newvalue)
{
	LLUUID obj_id;
	obj_id.set(newvalue.asString(), false);
	LLViewerObject::setDebugObjectId(obj_id);
	return true;
}

static bool handleDebugViewsChanged(const LLSD& newvalue)
{
	LLView::sDebugRects = newvalue.asBoolean();
	return true;
}

static bool handleFSFlushOnWriteChanged(const LLSD& newvalue)
{
	LLFile::sFlushOnWrite = newvalue.asBoolean();
	return true;
}

static bool handlePreciseLogTimestampsChanged(const LLSD& newvalue)
{
	bool enabled = newvalue.asBoolean();
	LLError::Log::sPreciseTimeStamp = enabled;
	llinfos << "Precise log file timestamps " << (enabled ? "en" : "dis")
			<< "abled." << llendl;
	return true;
}

static bool handleUserLogFileChanged(const LLSD& newvalue)
{
	std::string log_filename = newvalue.asString();
	LLFile::remove(log_filename);
	LLError::logToFile(log_filename);
	gAppViewerp->clearLogFilename();

	return true;
}

static bool handleUseAISForFetchingChanged(const LLSD& newvalue)
{
	LLInventoryModelFetch::setUseAISFetching(newvalue.asBoolean());
	return true;
}

static bool handleTextureFetchBoostWithFetchesChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("TextureFetchesBoostWithFetches");
	}
	return true;
}

static bool handleTextureFetchBoostWithSpeedChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("TextureFetchesBoostWithSpeed");
	}
	return true;
}

static bool handleFullResBoostedTexturesChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("TextureBoostedLoadFullRes");
	}
	return true;
}

static bool handleRestrainedLoveRelaxedTempAttachChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("RLVRelaxedTempAttach");
	}
	return true;
}

static bool handleRestrainedLoveAutomaticRenameItemsChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean())
	{
		gNotifications.add("RLVNoAttachmentAutoRename");
	}
	return true;
}

bool handleHideGroupTitleChanged(const LLSD& newvalue)
{
	gAgent.setHideGroupTitle(newvalue);
	return true;
}

bool handleDebugShowRenderInfoChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gPipeline.mNeedsDrawStats = true;
	}
	else if (!LLFloaterStats::findInstance())
	{
		gPipeline.mNeedsDrawStats = false;
	}
	return true;
}

bool handleEffectColorChanged(const LLSD& newvalue)
{
	gAgent.setEffectColor(LLColor4(newvalue));
	return true;
}

bool handleVoiceClientPrefsChanged(const LLSD& newvalue)
{
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.updateSettings();
	}
	return true;
}

static bool handleMiniMapCenterChanged(const LLSD& newvalue)
{
	LLPanelMiniMap::sMiniMapCenter = newvalue.asInteger();
	return true;
}

static bool handleMiniMapRotateChanged(const LLSD& newvalue)
{
	LLPanelMiniMap::sMiniMapRotate = newvalue.asBoolean();
	return true;
}

static bool handleNotecardEditorFontChanged(const LLSD&)
{
	LLPreviewNotecard::refreshCachedSettings();
	return true;
}

static bool handleScriptEditorFontChanged(const LLSD&)
{
	LLPreviewScript::refreshCachedSettings();
	return true;
}

static bool handleToolbarButtonsChanged(const LLSD&)
{
	if (gToolBarp)
	{
		gToolBarp->layoutButtons();
	}
	return true;
}

static bool handleSpellCheckChanged(const LLSD& newvalue)
{
	LLSpellCheck::getInstance()->setSpellCheck(gSavedSettings.getBool("SpellCheck"));
	LLSpellCheck::getInstance()->setShowMisspelled(gSavedSettings.getBool("SpellCheckShow"));
	LLSpellCheck::getInstance()->setDictionary(gSavedSettings.getString("SpellCheckLanguage"));
	return true;
}

static bool handleLanguageChanged(const LLSD& newvalue)
{
	gAgent.updateLanguage();
	return true;
}

static bool handleUseOldStatusBarIconsChanged(const LLSD&)
{
	if (gStatusBarp)
	{
		gStatusBarp->setIcons();
	}
	return true;
}

static bool handleSwapShoutWhisperShortcutsChanged(const LLSD& newvalue)
{
	LLChatBar::sSwappedShortcuts = newvalue.asBoolean();
	return true;
}

static bool handleSearchURLChanged(const LLSD& newvalue)
{
	// Do not propagate in OpenSim, since handleSearchURLChanged() is only
	// called for SL-specific settings (the search URL in OpenSim is set
	// via simulator features, not via saved settings).
	if (!gIsInSecondLife)
	{
		HBFloaterSearch::setSearchURL(newvalue.asString());
	}
	return true;
}

static bool handleVOCacheSettingChanged(const LLSD& newvalue)
{
	LLVOCacheEntry::updateSettings();
	return true;
}

static bool handleUse360InterestListSettingChanged(const LLSD& newvalue)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->setInterestListMode();
	}
	return true;
}

static bool handleShowPropLinesAtWaterSurfaceChanged(const LLSD&)
{
	// Force an update of the property lines
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		(*iter)->dirtyHeights();
	}
	return true;
}

static bool handleLightshareEnabledChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean() && LLStartUp::isLoggedIn())
	{
		gWLSkyParamMgr.processLightshareReset(true);
	}
	return true;
}

static bool handleUseLocalEnvironmentChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gWLSkyParamMgr.setDirty();
		gWLSkyParamMgr.animate(false);
		gSavedSettings.setBool("UseParcelEnvironment", false);
	}
	else
	{
		HBFloaterLocalEnv::closeInstance();
	}
	return true;
}

static bool handleUseParcelEnvironmentChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		LLFloaterWindlight::hideInstance();
		HBFloaterLocalEnv::closeInstance();
		gWLSkyParamMgr.setDirty();
		gWLSkyParamMgr.animate(false);
		gSavedSettings.setBool("UseLocalEnvironment", false);
		gSavedSettings.setBool("UseWLEstateTime", false);
		gEnvironment.clearEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_PARCEL,
											LLEnvironment::TRANSITION_INSTANT);
		if (gAutomationp)
		{
			gAutomationp->onWindlightChange("parcel", "", "");
		}
	}
	return true;
}

static bool handleUseWLEstateTimeChanged(const LLSD& newvalue)
{
	std::string time;
	if (newvalue.asBoolean())
	{
		gSavedSettings.setBool("UseLocalEnvironment", false);
		time = "region";
		LLEnvironment::setRegion();
	}
	else
	{
		time = "local";
	}
	if (gAutomationp)
	{
		gAutomationp->onWindlightChange(time, "", "");
	}
	return true;
}

static bool handlePrivateLookAtChanged(const LLSD& newvalue)
{
	LLHUDEffectLookAt::updateSettings();
	return true;
}

////////////////////////////////////////////////////////////////////////////

// Let's shorten the sources, improve their readability, and guard against
// crashes in case of a missing setting... HB
static void add_listener(const char* name, bool(&func)(const LLSD&))
{
	LLControlVariable* controlp = gSavedSettings.getControl(name);
	if (controlp)
	{
		controlp->getSignal()->connect(boost::bind(func, _2));
	}
	else
	{
		llwarns << "Could not find a global setting named: " << name << llendl;
	}
}

void settings_setup_listeners()
{
	// User interface related settings
	add_listener("ChatConsoleMaxLines", handleChatConsoleMaxLinesChanged);
	add_listener("ChatFontSize", handleChatFontSizeChanged);
	add_listener("ChatPersistTime", handleChatPersistTimeChanged);
	add_listener("DebugConsoleMaxLines", handleDebugConsoleMaxLinesChanged);
	add_listener("DebugObjectId", handleDebugObjectIdChanged);
	add_listener("DebugViews", handleDebugViewsChanged);
	add_listener("DisplayNamesUsage", handleDisplayNamesUsageChanged);
	add_listener("DisplayGamma", handleGammaChanged);
	add_listener("Language", handleLanguageChanged);
	add_listener("LanguageIsPublic", handleLanguageChanged);
	add_listener("LegacyNamesForFriends", handleLegacyNamesForFriendsChanged);
	add_listener("LegacyNamesForSpeakers",
				 handleLegacyNamesForSpeakersChanged);
	add_listener("MiniMapCenter", handleMiniMapCenterChanged);
	add_listener("MiniMapRotate", handleMiniMapRotateChanged);
	add_listener("NotecardEditorFont", handleNotecardEditorFontChanged);
	add_listener("OmitResidentAsLastName",
				 handleOmitResidentAsLastNameChanged);
	add_listener("ShowBuildButton", handleToolbarButtonsChanged);
	add_listener("ShowChatButton", handleToolbarButtonsChanged);
	add_listener("ShowFlyButton", handleToolbarButtonsChanged);
	add_listener("ShowFriendsButton", handleToolbarButtonsChanged);
	add_listener("ShowGroupsButton", handleToolbarButtonsChanged);
	add_listener("ShowIMButton", handleToolbarButtonsChanged);
	add_listener("ShowInventoryButton", handleToolbarButtonsChanged);
	add_listener("ShowMapButton", handleToolbarButtonsChanged);
	add_listener("ShowMiniMapButton", handleToolbarButtonsChanged);
	add_listener("ShowPropLinesAtWaterSurface",
				 handleShowPropLinesAtWaterSurfaceChanged);
	add_listener("ShowRadarButton", handleToolbarButtonsChanged);
	add_listener("ShowSearchButton", handleToolbarButtonsChanged);
	add_listener("ShowSnapshotButton", handleToolbarButtonsChanged);
	add_listener("StackMinimizedTopToBottom", handleStackMinimizedTopToBottom);
	add_listener("StackMinimizedRightToLeft", handleStackMinimizedRightToLeft);
	add_listener("StackScreenWidthFraction", handleStackScreenWidthFraction);
	add_listener("SwapShoutWhisperShortcuts",
				 handleSwapShoutWhisperShortcutsChanged);
	add_listener("SystemLanguage", handleLanguageChanged);
	add_listener("UseOldStatusBarIcons", handleUseOldStatusBarIconsChanged);
	add_listener("ScriptEditorFont", handleScriptEditorFontChanged);

	// Joystick related settings
	add_listener("JoystickAxis0", handleJoystickChanged);
	add_listener("JoystickAxis1", handleJoystickChanged);
	add_listener("JoystickAxis2", handleJoystickChanged);
	add_listener("JoystickAxis3", handleJoystickChanged);
	add_listener("JoystickAxis4", handleJoystickChanged);
	add_listener("JoystickAxis5", handleJoystickChanged);
	add_listener("JoystickAxis6", handleJoystickChanged);
	add_listener("FlycamAxisScale0", handleJoystickChanged);
	add_listener("FlycamAxisScale1", handleJoystickChanged);
	add_listener("FlycamAxisScale2", handleJoystickChanged);
	add_listener("FlycamAxisScale3", handleJoystickChanged);
	add_listener("FlycamAxisScale4", handleJoystickChanged);
	add_listener("FlycamAxisScale5", handleJoystickChanged);
	add_listener("FlycamAxisScale6", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone0", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone1", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone2", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone3", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone4", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone5", handleJoystickChanged);
	add_listener("FlycamAxisDeadZone6", handleJoystickChanged);
	add_listener("AvatarAxisScale0", handleJoystickChanged);
	add_listener("AvatarAxisScale1", handleJoystickChanged);
	add_listener("AvatarAxisScale2", handleJoystickChanged);
	add_listener("AvatarAxisScale3", handleJoystickChanged);
	add_listener("AvatarAxisScale4", handleJoystickChanged);
	add_listener("AvatarAxisScale5", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone0", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone1", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone2", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone3", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone4", handleJoystickChanged);
	add_listener("AvatarAxisDeadZone5", handleJoystickChanged);
	add_listener("BuildAxisScale0", handleJoystickChanged);
	add_listener("BuildAxisScale1", handleJoystickChanged);
	add_listener("BuildAxisScale2", handleJoystickChanged);
	add_listener("BuildAxisScale3", handleJoystickChanged);
	add_listener("BuildAxisScale4", handleJoystickChanged);
	add_listener("BuildAxisScale5", handleJoystickChanged);
	add_listener("BuildAxisDeadZone0", handleJoystickChanged);
	add_listener("BuildAxisDeadZone1", handleJoystickChanged);
	add_listener("BuildAxisDeadZone2", handleJoystickChanged);
	add_listener("BuildAxisDeadZone3", handleJoystickChanged);
	add_listener("BuildAxisDeadZone4", handleJoystickChanged);
	add_listener("BuildAxisDeadZone5", handleJoystickChanged);
	add_listener("NumpadControl", handleNumpadControlChanged);

	// Avatar related settings
	add_listener("AvatarOffsetZ", handleAvatarOffsetChanged);
	add_listener("AvatarPhysics", handleAvatarPhysicsChanged);
	add_listener("OSAllowBakeOnMeshUploads", handleBakeOnMeshUploadsChanged);
	add_listener("PuppetryAllowed", handlePuppetryAllowedChanged);

	// Camera related settings
	add_listener("CameraIgnoreCollisions", handleCameraCollisionsChanged);
	add_listener("CameraFrontView", handleCameraChanged);
	add_listener("CameraOffsetDefault", handleCameraChanged);
	add_listener("FirstPersonAvatarVisible", handleAvatarDebugSettingsChanged);
	add_listener("FocusOffsetDefault", handleCameraChanged);
	add_listener("FocusOffsetFrontView", handleCameraChanged);
	add_listener("CameraOffsetFrontView", handleCameraChanged);
	add_listener("TrackFocusObject", handleTrackFocusObjectChanged);

	// Rendering related settings
	add_listener("DebugShowRenderInfo", handleDebugShowRenderInfoChanged);
	add_listener("EffectColor", handleEffectColorChanged);
	add_listener("OctreeStaticObjectSizeFactor", handleRepartition);
	add_listener("OctreeDistanceFactor", handleRepartition);
	add_listener("OctreeMaxNodeCapacity", handleRepartition);
	add_listener("OctreeMinimumNodeSize", handleRepartition);
	add_listener("OctreeAlphaDistanceFactor", handleRepartition);
	add_listener("OctreeAttachmentSizeFactor", handleRepartition);
	add_listener("RenderAnimateTrees", handleResetVertexBuffersChanged);
	add_listener("RenderAutoMaskAlphaDeferred",
				 handleResetVertexBuffersChanged);
	add_listener("RenderAutoMaskAlphaNonDeferred",
				 handleResetVertexBuffersChanged);
	add_listener("RenderAvatarCloth", handleSetShaderChanged);
	add_listener("RenderAvatarLODFactor", handleAvatarDebugSettingsChanged);
	add_listener("RenderAvatarMaxNonImpostors",
				 handleAvatarDebugSettingsChanged);
	add_listener("RenderAvatarMaxPuppets", handleAvatarDebugSettingsChanged);
	add_listener("RenderAvatarPhysicsLODFactor",
				 handleAvatarDebugSettingsChanged);
	add_listener("RenderBatchedGlyphs", handleRenderBatchedGlyphsChanged);
	add_listener("RenderCompressTextures",
				 handleRenderCompressTexturesChanged);
	add_listener("RenderCompressThreshold",
				 handleRenderCompressTexturesChanged);
	add_listener("RenderDeferred", handleRenderDeferredChanged);
	add_listener("RenderDeferredNoise", handleGLBufferChanged);
	add_listener("RenderDeferredSSAO", handleSetShaderChanged);
	add_listener("RenderDepthOfField", handleGLBufferChanged);
	add_listener("RenderDynamicLOD", handleRenderDynamicLODChanged);
	add_listener("RenderFarClip", handleRenderFarClipChanged);
	add_listener("RenderFlexTimeFactor", handleFlexLODChanged);
#if 0	// This should only taken into account after a restart. HB
	add_listener("RenderFSAASamples", handleGLBufferChanged);
#endif
	add_listener("RenderDeferredAAQuality", handleGLBufferChanged);
	add_listener("RenderDeferredDisplayGamma", handleSetShaderChanged);
	add_listener("RenderGLImageSyncInThread", handleRenderGLImageSyncInThread);
	add_listener("RenderGlow", handleGLBufferChanged);
	add_listener("RenderGlowResolutionPow", handleGLBufferChanged);
	add_listener("RenderHideGroupTitle", handleHideGroupTitleChanged);
	add_listener("RenderHideGroupTitleAll", handleAvatarDebugSettingsChanged);
	add_listener("RenderMaxPartCount", handleMaxPartCountChanged);
	add_listener("RenderMaxTextureIndex", handleSetShaderChanged);
	add_listener("RenderUseDepthClamp", handleSetShaderChanged);
	add_listener("RenderMaxVBOSize", handleResetVertexBuffersChanged);
	add_listener("RenderName", handleAvatarDebugSettingsChanged);
	add_listener("RenderOptimizeMeshVertexCache",
				 handleRenderOptimizeMeshVertexCacheChanged);
	add_listener("RenderReflectionsEnabled", handleReflectionProbesChanged);
	add_listener("RenderReflectionProbeDetail", handleReflectionProbesChanged);
	add_listener("RenderReflectionProbeLevel",
				 handleReflectionProbesChanged);
	add_listener("RenderReflectionProbeResolution",
				 handleReflectionProbesChanged);
	add_listener("RenderResolutionDivisor",
				 handleRenderResolutionDivisorChanged);
	add_listener("RenderScreenSpaceReflections",
				 handleReflectionProbesChanged);
	add_listener("RenderShadowDetail", handleSetShaderChanged);
	add_listener("RenderShadowResolutionScale", handleShadowsResized);
	add_listener("RenderSpecularExponent", handleLUTBufferChanged);
	add_listener("RenderSpecularResX", handleLUTBufferChanged);
	add_listener("RenderSpecularResY", handleLUTBufferChanged);
	add_listener("RenderTerrainLODFactor", handleTerrainLODChanged);
	add_listener("RenderTransparentWater", handleSetShaderChanged);
	add_listener("RenderWaterReflectionType",
				 handleRenderWaterReflectionTypeChanged);
	add_listener("RenderTreeAnimationDamping", handleTreeSettingsChanged);
	add_listener("RenderTreeTrunkStiffness", handleTreeSettingsChanged);
	add_listener("RenderTreeWindSensitivity", handleTreeSettingsChanged);
	add_listener("RenderTreeLODFactor", handleTreeSettingsChanged);
	add_listener("RenderGLUseVBCache", handleRenderGLUseVBCacheChanged);
	add_listener("RenderVolumeLODFactor", handleVolumeSettingsChanged);
	add_listener("SkyUseClassicClouds", handleSkyUseClassicCloudsChanged);
	add_listener("UseOcclusion", handleUseOcclusionChanged);
	add_listener("WLSkyDetail", handleWLSkyDetailChanged);

	// Network related settings
	add_listener("EnableHTTP2", handleEnableHTTP2Changed);
	add_listener("InterpolationTime", handleInterpolationTimesChanged);
	add_listener("InterpolationPhaseOut", handleInterpolationTimesChanged);
	add_listener("RegionCrossingInterpolationTime",
				 handleInterpolationTimesChanged);
	add_listener("MeshMaxConcurrentRequests",
				 handleMeshMaxConcurrentRequestsChanged);
	add_listener("NoVerifySSLCert", handleNoVerifySSLCertChanged);
	add_listener("PingInterpolate", handlePingInterpolateChanged);
	add_listener("SearchURL", handleSearchURLChanged);
	add_listener("ThrottleBandwidthKbps", handleBandwidthChanged);
	add_listener("VelocityInterpolate", handleVelocityInterpolateChanged);

	// Obects cache related settings
	add_listener("BiasedObjectRetention", handleVOCacheSettingChanged);
	add_listener("NonVisibleObjectsInMemoryTime", handleVOCacheSettingChanged);
	add_listener("SceneLoadMinRadius", handleVOCacheSettingChanged);
	add_listener("SceneLoadFrontPixelThreshold", handleVOCacheSettingChanged);
	add_listener("SceneLoadRearPixelThreshold", handleVOCacheSettingChanged);
	add_listener("SceneLoadRearMaxRadiusFraction",
				 handleVOCacheSettingChanged);
	add_listener("Use360InterestList", handleUse360InterestListSettingChanged);

	// Audio and media related settings
	add_listener("AudioLevelMaster", handleAudioVolumeChanged);
	add_listener("AudioLevelSFX", handleAudioVolumeChanged);
	add_listener("AudioLevelUI", handleAudioVolumeChanged);
	add_listener("AudioLevelAmbient", handleAudioVolumeChanged);
	add_listener("AudioLevelMic", handleVoiceClientPrefsChanged);
	add_listener("AudioLevelMusic", handleAudioVolumeChanged);
	add_listener("AudioLevelMedia", handleAudioVolumeChanged);
	add_listener("AudioLevelVoice", handleAudioVolumeChanged);
	add_listener("AudioLevelDoppler", handleAudioVolumeChanged);
	add_listener("AudioLevelRolloff", handleAudioVolumeChanged);
	add_listener("AudioLevelUnderwaterRolloff", handleAudioVolumeChanged);
	add_listener("AudioLevelWind", handleAudioVolumeChanged);
	add_listener("DisableWindAudio", handleAudioVolumeChanged);
	add_listener("EnableStreamingMusic", handleAudioStreamMusicChanged);
	add_listener("EnableStreamingMedia", handlePrimMediaChanged);
	add_listener("PrimMediaMasterEnabled", handlePrimMediaChanged);
	add_listener("MuteAudio", handleAudioVolumeChanged);
	add_listener("MuteMusic", handleAudioVolumeChanged);
	add_listener("MuteMedia", handleAudioVolumeChanged);
	add_listener("MuteVoice", handleAudioVolumeChanged);
	add_listener("MuteAmbient", handleAudioVolumeChanged);
	add_listener("MuteUI", handleAudioVolumeChanged);

	// Voice related settings
	add_listener("EnableVoiceChat", handleVoiceClientPrefsChanged);
	add_listener("LipSyncEnabled", handleVoiceClientPrefsChanged);
	add_listener("PTTCurrentlyEnabled", handleVoiceClientPrefsChanged);
	add_listener("PushToTalkButton", handleVoiceClientPrefsChanged);
	add_listener("PushToTalkToggle", handleVoiceClientPrefsChanged);
	add_listener("VoiceEarLocation", handleVoiceClientPrefsChanged);
	add_listener("VoiceInputAudioDevice", handleVoiceClientPrefsChanged);
	add_listener("VoiceOutputAudioDevice", handleVoiceClientPrefsChanged);

	// Memory related settings
	add_listener("MaxBoundTexMem", handleVideoMemoryChanged);
	add_listener("TextureMemory", handleVideoMemoryChanged);
	add_listener("TexMemMultiplier", handleVideoMemoryChanged);
	add_listener("VRAMOverride", handleVideoMemoryChanged);

	// Spell checking related settings
	add_listener("SpellCheck", handleSpellCheckChanged);
	add_listener("SpellCheckShow", handleSpellCheckChanged);
	add_listener("SpellCheckLanguage", handleSpellCheckChanged);

	// Environment related settings
	add_listener("LightshareEnabled", handleLightshareEnabledChanged);
	add_listener("UseLocalEnvironment", handleUseLocalEnvironmentChanged);
	add_listener("UseParcelEnvironment", handleUseParcelEnvironmentChanged);
	add_listener("UseWLEstateTime", handleUseWLEstateTimeChanged);

	// Privacy related settings
	add_listener("PrivateLookAt", handlePrivateLookAtChanged);
	add_listener("PrivateLookAtLimit", handlePrivateLookAtChanged);

	// Miscellaneous settings
	add_listener("AutoReloadFailedPatchTexDelay",
				 handleAutoReloadFailedPatchTexDelayChanged);
	add_listener("DebugPermissions", handleDebugPermissionsChanged);
#if LL_FAST_TIMERS_ENABLED
	add_listener("FastTimersAlwaysEnabled",
				 handleFastTimersAlwaysEnabledChanged);
#endif
	add_listener("FSFlushOnWrite", handleFSFlushOnWriteChanged);
	add_listener("HighResSnapshot", handleHighResSnapshotChanged);
	add_listener("TextureFetchBoostWithFetches",
				 handleTextureFetchBoostWithFetchesChanged);
	add_listener("TextureFetchBoostWithSpeed",
				 handleTextureFetchBoostWithSpeedChanged);
	add_listener("FullResBoostedTextures",
				 handleFullResBoostedTexturesChanged);
//MK
	add_listener("RestrainedLoveRelaxedTempAttach",
				 handleRestrainedLoveRelaxedTempAttachChanged);
	add_listener("RestrainedLoveAutomaticRenameItems",
				 handleRestrainedLoveAutomaticRenameItemsChanged);
//mk
	add_listener("PreciseLogTimestamps", handlePreciseLogTimestampsChanged);
	add_listener("UserLogFile", handleUserLogFileChanged);
	add_listener("UseAISForFetching", handleUseAISForFetchingChanged);
}
