/**
 * @file lldaeloader.h
 * @brief LLDAELoader class definition
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

#ifndef LL_LLDAELOADER_H
#define LL_LLDAELOADER_H

#include "llmodelloader.h"

class DAE;
class daeElement;
class domProfile_COMMON;
class domInstance_geometry;
class domNode;
class domTranslate;
class domController;
class domSkin;
class domMesh;

class LLDAELoader final : public LLModelLoader
{
public:
	typedef std::map<std::string, LLImportMaterial> material_map;
	typedef std::map<daeElement*, std::vector<LLPointer<LLModel> > > dae_model_map;
	dae_model_map mModelsMap;

	LLDAELoader(const std::string& filename, S32 lod,
				LLModelLoader::load_callback_t load_cb,
				LLModelLoader::joint_lookup_func_t joint_lookup_func,
				LLModelLoader::texture_load_func_t texture_load_func,
				LLModelLoader::state_callback_t state_cb, void* userdata,
				JointTransformMap& joint_transform_map,
				JointNameSet& joints_from_nodes,
				std::map<std::string, std::string>& joint_alias_map,
				U32 max_joints_per_mesh, U32 model_limit, bool preprocess);

	bool openFile(const std::string& filename) override;

protected:
	void processElement(daeElement* element, bool& badElement, DAE* dae);
	void processDomModel(LLModel* modelp, DAE* dae, daeElement* rootp,
						 domMesh* meshp, domSkin* skinp);

	material_map getMaterials(LLModel* model,
							  domInstance_geometry* instance_geo, DAE* dae);
	LLImportMaterial profileToMaterial(domProfile_COMMON* material, DAE* dae);
	LLColor4 getDaeColor(daeElement* element);

	daeElement* getChildFromElement(daeElement* elementp,
									const std::string& name);

	bool isNodeAJoint(domNode* nodep);
	void processJointNode(domNode* nodep,
						  std::map<std::string,LLMatrix4>& jointTransforms);
	void extractTranslation(domTranslate* translatep, LLMatrix4& transform);
	void extractTranslationViaElement(daeElement* translate_elemp,
									  LLMatrix4& transform);
	void extractTranslationViaSID(daeElement* elementp, LLMatrix4& transform);
	void buildJointToNodeMappingFromScene(daeElement* rootp);
	void processJointToNodeMapping(domNode* nodep);
	void processChildJoints(domNode* parent_nodep);

	bool verifyCount(S32 expected, S32 result);

	// Verify that a controller matches vertex counts
	bool verifyController(domController* controllerp);

	static bool addVolumeFacesFromDomMesh(LLModel* modelp, domMesh* meshp,
										  LLSD& log_msg);

	// Loads a mesh breaking it into one or more models as necessary to get
	// around volume face limitations while retaining > 8 materials
	bool loadModelsFromDomMesh(domMesh* meshp,
							   std::vector<LLModel*>& models_out,
							   U32 submodel_limit);

	static std::string getElementLabel(daeElement* elementp);

	static size_t getSuffixPosition(const std::string& label);
	static std::string getLodlessLabel(daeElement* elementp);

	static std::string preprocessDAE(const std::string& filename);

private:
	// Attempt to limit amount of generated submodels
	U32		mGeneratedModelLimit;

	bool	mPreprocessDAE;
};

#endif  // LL_LLDAELLOADER_H
