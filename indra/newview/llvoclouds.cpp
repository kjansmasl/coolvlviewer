/**
 * @file llvoclouds.cpp
 * @brief Implementation of LLVOClouds class which is a derivation fo LLViewerObject
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

#include "llvoclouds.h"

#include "imageids.h"
#include "llfasttimer.h"
#include "llprimitive.h"

#include "llagent.h"		// to get camera position
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "llenvironment.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvosky.h"
#include "llworld.h"

LLUUID gCloudTextureID = IMG_CLOUD_POOF;

///////////////////////////////////////////////////////////////////////////////
// LLVOClouds class
///////////////////////////////////////////////////////////////////////////////

LLVOClouds::LLVOClouds(const LLUUID& id, LLViewerRegion* regionp)
:	LLAlphaObject(id, LL_VO_CLOUDS, regionp)
{
	mCloudGroupp = NULL;
	mCanSelect = false;
	setNumTEs(1);

	LLViewerTexture* image;
	if (gCloudTextureID != IMG_CLOUD_POOF ||
		LLViewerFetchedTexture::sDefaultCloudsImagep.isNull())
	{
		image =
			LLViewerTextureManager::getFetchedTexture(gCloudTextureID,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_CLOUDS);
#if !LL_IMPLICIT_SETNODELETE
		image->setNoDelete();
#endif
	}
	else
	{
		image = LLViewerFetchedTexture::sDefaultCloudsImagep.get();
	}
	setTEImage(0, image);
}

void LLVOClouds::idleUpdate(F64 time)
{
	if (mDrawable && gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
	{
		// Set rebuild flag (so that the renderer will rebuild the primitive)
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	}
}

void LLVOClouds::setPixelAreaAndAngle()
{
	mAppAngle = 50;
	mPixelArea = 1500 * 100;
}

void LLVOClouds::updateTextures()
{
	getTEImage(0)->addTextureStats(mPixelArea);
}

LLDrawable* LLVOClouds::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_CLOUDS);
	return mDrawable;
}

bool LLVOClouds::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_CLOUDS);

	S32 num_parts = mCloudGroupp->getNumPuffs();
	LLSpatialGroup* group = drawable->getSpatialGroup();
	if (!group && num_parts)
	{
		drawable->movePartition();
		group = drawable->getSpatialGroup();
	}

	if (group && group->isVisible())
	{
		dirtySpatialGroup();
	}

	if (!num_parts)
	{
		if (group && drawable->getNumFaces())
		{
			group->setState(LLSpatialGroup::GEOM_DIRTY);
		}
		drawable->setNumFaces(0, NULL, getTEImage(0));
		return true;
	}

 	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
	{
		return true;
	}

	if (num_parts > drawable->getNumFaces())
	{
		drawable->setNumFacesFast(num_parts + num_parts / 4, NULL,
								  getTEImage(0));
	}

	mDepth = (getPositionAgent() - gViewerCamera.getOrigin()) *
			 gViewerCamera.getAtAxis();

	LLFace* facep;
	S32 face_indx = 0;
	// Cloud color based on Sun (or Moon) color and ambient
	LLColor3 cloud_color;
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)
	{
		cloud_color = skyp->getLightDiffuse() +
					  LLColor3(skyp->getTotalAmbient());
	}
	for ( ;	face_indx < num_parts; face_indx++)
	{
		facep = drawable->getFace(face_indx);
		if (!facep)
		{
			continue;
		}

		facep->setTEOffset(face_indx);
		facep->setSize(4, 6);

		facep->setViewerObject(this);

		const LLCloudPuff& puff = mCloudGroupp->getPuff(face_indx);
		facep->mCenterLocal =
			gAgent.getPosAgentFromGlobal(puff.getPositionGlobal());
		facep->setFaceColor(LLColor4(cloud_color, puff.getAlpha()));

		facep->setDiffuseMap(getTEImage(0));
	}
	for (S32 count = drawable->getNumFaces(); face_indx < count; ++face_indx)
	{
		facep = drawable->getFace(face_indx);
		if (facep)
		{
			facep->setTEOffset(face_indx);
			facep->setSize(0, 0);
		}
	}

	drawable->movePartition();

	return true;
}

F32 LLVOClouds::getPartSize(S32 idx)
{
	return (CLOUD_PUFF_HEIGHT + CLOUD_PUFF_WIDTH) * 0.5f;
}

void LLVOClouds::getGeometry(S32 idx,
							 LLStrider<LLVector4a>& verticesp,
							 LLStrider<LLVector3>& normalsp,
							 LLStrider<LLVector2>& texcoordsp,
							 LLStrider<LLColor4U>& colorsp,
							 LLStrider<LLColor4U>& emissivep,
							 LLStrider<U16>& indicesp)
{

	if (idx >= mCloudGroupp->getNumPuffs())
	{
		return;
	}

	LLDrawable* drawable = mDrawable;
	LLFace* facep = drawable->getFace(idx);

	if (!facep || !facep->hasGeometry())
	{
		return;
	}

	const LLCloudPuff& puff = mCloudGroupp->getPuff(idx);

	LLColor3 cloud_color;
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)
	{
		cloud_color = skyp->getLightDiffuse() +
					  LLColor3(skyp->getTotalAmbient());
	}
	LLColor4 float_color(cloud_color, puff.getAlpha());
	facep->setFaceColor(float_color);

	LLVector4a part_pos_agent;
	part_pos_agent.load3(facep->mCenterLocal.mV);
	LLVector4a at;
	at.load3(gViewerCamera.getAtAxis().mV);
	LLVector4a up(0.f, 0.f, 1.f);
	LLVector4a right;

	right.setCross3(at, up);
	right.normalize3fast();
	up.setCross3(right, at);
	up.normalize3fast();
	right.mul(0.5f * CLOUD_PUFF_WIDTH);
	up.mul(0.5f * CLOUD_PUFF_HEIGHT);

	LLVector3 normal(0.f, 0.f, -1.f);

	// HACK: the verticesp->mV[3] = 0.f here are to set the texture index to 0
	// (particles don't use texture batching, maybe they should) this works
	// because there is actually a 4th float stored after the vertex position
	// which is used as a texture index.

	LLVector4a ppapu;
	LLVector4a ppamu;

	ppapu.setAdd(part_pos_agent, up);
	ppamu.setSub(part_pos_agent, up);

	verticesp->setSub(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setSub(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;

	LLColor4U color;
	color.set(float_color);
	*colorsp++ = color;
	*colorsp++ = color;
	*colorsp++ = color;
	*colorsp++ = color;

	*normalsp++ = normal;
	*normalsp++ = normal;
	*normalsp++ = normal;
	*normalsp++ = normal;
}

U32 LLVOClouds::getPartitionType() const
{
	return LLViewerRegion::PARTITION_CLOUD;
}

// virtual
void LLVOClouds::updateDrawable(bool force_damped)
{
	// Force an immediate rebuild on any update
	if (mDrawable.notNull())
	{
		mDrawable->updateXform(true);
		gPipeline.markRebuild(mDrawable);
	}
	clearChanged(SHIFTED);
}

///////////////////////////////////////////////////////////////////////////////
// LLCloudPartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLCloudPartition::LLCloudPartition(LLViewerRegion* regionp)
:	LLParticlePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_CLOUDS;
	mPartitionType = LLViewerRegion::PARTITION_CLOUD;
}
