/**
 * @file llviewerobjectexport.h
 * @authors Latif Khalifa (DAE exporter) / Apelsin & Lirusaito (wavefront
 *          exporter). Backported/adapted/optimized by Henri Beauchamp.
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
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

#ifndef LL_LLVIEWEROBJECTEXPORT_H
#define LL_LLVIEWEROBJECTEXPORT_H

#include <string>
#include <vector>

#include "dom/domElements.h"

#include "hbfastmap.h"
#include "llfile.h"
#include "hbfileselector.h"
#include "llsd.h"
#include "lltextureentry.h"
#include "lluuid.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLFace;
class LLPolyMesh;
class LLSelectNode;
class LLUIString;
class LLViewerObject;
class LLVOAvatar;
class LLVolume;
class LLVolumeFace;
class LLXform;

///////////////////////////////////////////////////////////////////////////////
// Collada exporter
///////////////////////////////////////////////////////////////////////////////

class LKDAESaver
{
protected:
	LOG_CLASS(LKDAESaver);

public:
	LKDAESaver()									{}
	~LKDAESaver()									{}

	bool addSelectedObjects(std::string& root_name, U32& total);
	bool saveDAE(std::string filename);

	enum image_format_type
	{
		ft_tga,
		ft_png,
		ft_j2c,
		ft_bmp,
		ft_jpg
	};
	static const std::string image_format_ext[];

	class MaterialInfo
	{
	public:
		LL_INLINE bool matches(LLTextureEntry* te)
		{
			return mTextureID == te->getID() && mColor == te->getColor();
		}

		LL_INLINE bool operator==(const MaterialInfo& rhs)
		{
			return mTextureID == rhs.mTextureID && mColor == rhs.mColor &&
				   mName == rhs.mName;
		}

		LL_INLINE bool operator!=(const MaterialInfo& rhs)
		{
			return mTextureID != rhs.mTextureID || mColor != rhs.mColor ||
				   mName != rhs.mName;
		}

		MaterialInfo(const LLUUID& tex_id, const LLColor4& color,
					 const std::string& name)
		:	mTextureID(tex_id),
			mColor(color),
			mName(name)
		{
		}

		MaterialInfo(const MaterialInfo& rhs)
		{
			mTextureID = rhs.mTextureID;
			mColor = rhs.mColor;
			mName = rhs.mName;
		}

		MaterialInfo& operator= (const MaterialInfo& rhs)
		{
			mTextureID = rhs.mTextureID;
			mColor = rhs.mColor;
			mName = rhs.mName;
			return *this;
		}

	public:
		LLUUID		mTextureID;
		LLColor4	mColor;
		std::string	mName;
	};

	typedef std::vector<std::pair<LLViewerObject*, std::string> > obj_info_t;
	typedef std::vector<std::string> string_list_t;
	typedef std::vector<S32> int_list_t;
	typedef std::vector<MaterialInfo> material_list_t;

private:
	void add(LLViewerObject* prim, const std::string& name);
	void transformTexCoord(S32 num_vert, LLVector2* coord,
						   LLVector3* positions, LLVector3* normals,
						   LLTextureEntry* te, LLVector3 scale);
	void addSource(daeElement* mesh, const char* src_id, std::string params,
				   const std::vector<F32>& vals);
	void addPolygons(daeElement* mesh, const char* geomID,
					 const char* mat_id, LLViewerObject* obj,
					 int_list_t* faces_to_include);
	bool skipFace(LLTextureEntry* te);
	MaterialInfo getMaterial(LLTextureEntry* te);
	void getMaterials(LLViewerObject* obj, material_list_t* ret);
	void getFacesWithMaterial(LLViewerObject* obj,
							  MaterialInfo& mat, int_list_t* ret);
	void generateEffects(daeElement* effects);
	void generateImagesSection(daeElement* images);

	void updateTextureInfo();

public:
	LLVector3		mOffset;
	S32				mTotalNumMaterials;
	material_list_t	mAllMaterials;
	uuid_vec_t		mTextures;
	string_list_t	mTextureNames;
	obj_info_t		mObjects;
	std::string		mImageFormat;
};

///////////////////////////////////////////////////////////////////////////////
// Floater for the Collada exporter
///////////////////////////////////////////////////////////////////////////////

class LKFloaterColladaExport final
:	public LLFloater, public LLFloaterSingleton<LKFloaterColladaExport>
{
	friend class LLUISingleton<LKFloaterColladaExport,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LKFloaterColladaExport);

public:
	~LKFloaterColladaExport() override;

	bool postBuild() override;

	static void saveTexturesWorker(void* data);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LKFloaterColladaExport(const LLSD&);

	void addSelectedObjects();
	void updateTitleProgress();
	static void onTextureExportCheck(LLUICtrl* ctrl, void* data);
	static void onClickExport(void* data);
	static void filePickerCallback(HBFileSelector::ESaveFilter type,
								   std::string& filename, void* data);
	void saveTextures();
	void saveDAE();

private:
	LLButton*		mExportButton;
	LLCheckBoxCtrl*	mTextureExportCheck;
	LLComboBox*		mTextureTypeCombo;

	U32				mTotal;
	U32				mNumTextures;
	U32				mNumExportableTextures;

	LLTimer			mTimer;

	LKDAESaver		mSaver;

	std::string		mTitle;
	std::string		mObjectName;
	std::string		mFilename;
	std::string		mFolder;

	typedef fast_hmap<LLUUID, std::string> texture_list_t;
	texture_list_t	mTexturesToSave;
};

///////////////////////////////////////////////////////////////////////////////
// Wavefront exporter
///////////////////////////////////////////////////////////////////////////////

// Avatar exporting plain does not work: the avatar data istelf is screwed and
// the resulting OBJ file would nott reproduce the avatar after loaded in any
// 3D modelling program (I tried with Blender, Wings 3D and a couple others, to
// no avail): it just reproduces a few half-spheres !
// The attachments cannot be exported either with the algorithm used here
// because the latter relies on select node permissions to be received for each
// attachment prim already, while it cannot happen on the same frame since when
// selection is done a message is sent to the server and the reply arrives only
// a few milliseconds later: to get attachments export working, the same idle
// callbacks based export worker algorithm as in hbobjectbackup.cpp should be
// implemented instead (but it does not make sense implementing it either as
// long as the avatar OBJ export itself is not fixed). HB
#define LL_EXPORT_AVATAR_OBJ 0

typedef std::vector<std::pair<LLVector3, LLVector2> > vert_t;
typedef std::vector<LLVector3> vec3_t;

struct tri
{
	tri(int a, int b, int c) : v0(a), v1(b), v2(c)	{}
	int v0;
	int v1;
	int v2;
};
typedef std::vector<tri> tri_t;

class ALWavefront
{
public:
	ALWavefront(vert_t v, tri_t t);
	ALWavefront(const LLVolumeFace* face, const LLXform* transform = NULL,
				const LLXform* transform_normals = NULL);
	ALWavefront(LLFace* face, LLPolyMesh* mesh = NULL,
				const LLXform* transform = NULL,
				const LLXform* transform_normals = NULL);

	static void Transform(vert_t& v, const LLXform* x);
	static void Transform(vec3_t& v, const LLXform* x);

public:
	vert_t		mVertices;
	vec3_t		mNormals;	// null unless otherwise specified !
	tri_t		mTriangles;	// because almost all surfaces in SL are triangles
	std::string	mName;
};

class ALWavefrontSaver
{
public:
	ALWavefrontSaver()								{}
	~ALWavefrontSaver()								{}

	static void exportSelection();
#if LL_EXPORT_AVATAR_OBJ
	static void exportAvatar(bool with_attachments = false);
#endif

private:
	void add(const ALWavefront& obj);
	void add(const LLVolume* vol, const LLXform* transform = NULL,
			 const LLXform* transform_normals = NULL);
	void add(const LLViewerObject* some_vo);
#if LL_EXPORT_AVATAR_OBJ
	bool add(const LLVOAvatar* av_vo, bool with_attachments);
#endif

	bool saveToFile(LLFILE* fp);

	static void saveOpenPicker(ALWavefrontSaver* wfsaver, std::string name);

	static void saveNotificationCallback(const LLSD& notification,
										 const LLSD& response,
										 ALWavefrontSaver* wfsaver,
										 std::string name);

	static void savePickerCallback(HBFileSelector::ESaveFilter type,
								   std::string& filename, void* userdata);

private:
	LLVector3					mOffset;
	std::vector<ALWavefront>	mWavefrontObjects;
};

#endif	// LL_LLVIEWEROBJECTEXPORT_H
