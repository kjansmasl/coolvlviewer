/**
 * @file llbvhloader.h
 * @brief Translates a BVH files to LindenLabAnimation format.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLBVHLOADER_H
#define LL_LLBVHLOADER_H

#include "llbvhconsts.h"
#include "llfile.h"
#include "llmath.h"
#include "llpreprocessor.h"
#include "llmatrix3.h"
#include "llvector3.h"

class LLDataPacker;

constexpr S32 BVH_PARSER_LINE_SIZE = 2048;

struct Key
{
	Key()
	{
		mPos[0] = mPos[1] = mPos[2] = 0.f;
		mRot[0] = mRot[1] = mRot[2] = 0.f;
		mIgnorePos = false;
		mIgnoreRot = false;
	}

	F32		mPos[3];
	F32		mRot[3];
	bool	mIgnorePos;
	bool	mIgnoreRot;
};

typedef  std::vector<Key> KeyVector;

struct Joint
{
	Joint(const char* name)
	{
		mName = name;
		mIgnore = false;
		mIgnorePositions = false;
		mRelativePositionKey = false;
		mRelativeRotationKey = false;
		mOutName = name;
		mOrder[0] = 'X';
		mOrder[1] = 'Y';
		mOrder[2] = 'Z';
		mOrder[3] = 0;
		mNumPosKeys = 0;
		mNumRotKeys = 0;
		mChildTreeMaxDepth = 0;
		mPriority = 0;
		mNumChannels = 3;
	}

	// Include aligned members first
	LLMatrix3		mFrameMatrix;
	LLMatrix3		mOffsetMatrix;
	LLVector3		mRelativePosition;
	//
	std::string		mName;
	bool			mIgnore;
	bool			mIgnorePositions;
	bool			mRelativePositionKey;
	bool			mRelativeRotationKey;
	std::string		mOutName;
	std::string		mMergeParentName;
	std::string		mMergeChildName;
	char			mOrder[4];
	KeyVector		mKeys;
	S32				mNumPosKeys;
	S32				mNumRotKeys;
	S32				mChildTreeMaxDepth;
	S32				mPriority;
	S32				mNumChannels;
};

struct Constraint
{
	char			mSourceJointName[16];
	char			mTargetJointName[16];
	S32				mChainLength;
	LLVector3		mSourceOffset;
	LLVector3		mTargetOffset;
	LLVector3		mTargetDir;
	F32				mEaseInStart;
	F32				mEaseInStop;
	F32				mEaseOutStart;
	F32				mEaseOutStop;
	EConstraintType mConstraintType;
};

typedef std::vector<Joint*> JointVector;
typedef std::vector<Constraint> ConstraintVector;

class Translation
{
public:
	Translation()
	{
		mIgnore = false;
		mIgnorePositions = false;
		mRelativePositionKey = false;
		mRelativeRotationKey = false;
		mPriorityModifier = 0;
	}

public:
	LLMatrix3	mFrameMatrix;
	LLMatrix3	mOffsetMatrix;
	LLVector3	mRelativePosition;
	S32			mPriorityModifier;
	bool		mIgnore;
	bool		mIgnorePositions;
	bool		mRelativePositionKey;
	bool		mRelativeRotationKey;
	std::string	mOutName;
	std::string	mMergeParentName;
	std::string	mMergeChildName;
};

typedef enum e_load_status
{
	E_ST_OK,
	E_ST_EOF,
	E_ST_NO_CONSTRAINT,
	E_ST_NO_FILE,
	E_ST_NO_HIER,
	E_ST_NO_JOINT,
	E_ST_NO_NAME,
	E_ST_NO_OFFSET,
	E_ST_NO_CHANNELS,
	E_ST_NO_ROTATION,
	E_ST_NO_AXIS,
	E_ST_NO_MOTION,
	E_ST_NO_FRAMES,
	E_ST_NO_FRAME_TIME,
	E_ST_NO_POS,
	E_ST_NO_ROT,
	E_ST_NO_XLT_FILE,
	E_ST_NO_XLT_HEADER,
	E_ST_NO_XLT_NAME,
	E_ST_NO_XLT_IGNORE,
	E_ST_NO_XLT_RELATIVE,
	E_ST_NO_XLT_OUTNAME,
	E_ST_NO_XLT_MATRIX,
	E_ST_NO_XLT_MERGECHILD,
	E_ST_NO_XLT_MERGEPARENT,
	E_ST_NO_XLT_PRIORITY,
	E_ST_NO_XLT_LOOP,
	E_ST_NO_XLT_EASEIN,
	E_ST_NO_XLT_EASEOUT,
	E_ST_NO_XLT_HAND,
	E_ST_NO_XLT_EMOTE,
	E_ST_BAD_ROOT
} ELoadStatus;

typedef std::map<std::string, Translation> TranslationMap;

class LLBVHLoader
{
	friend class LLKeyframeMotion;

protected:
	LOG_CLASS(LLBVHLoader);

public:
	LLBVHLoader(const char* buffer, ELoadStatus& load_status, S32& error_line,
				std::map<std::string, std::string>& joint_alias_map);
	~LLBVHLoader();

	// Loads the specified translation table.
	ELoadStatus loadTranslationTable(const char* fileName);

	// Creates a new joint alias.
	void makeTranslation(const std::string& key, const std::string& value);

#if 0	// Not used
	// Loads joint aliases from XML file.
	ELoadStatus loadAliases(const char* filename);
#endif

	// Loads the specified BVH file and returns a status code.
	ELoadStatus loadBVHFile(const char* buffer, char* err_text, S32& err_line);

	// For debug log level info
	void dumpBVHInfo();

	// Applies translations to BVH data loaded.
	void applyTranslations();

	// Returns the number of lines scanned.
	// Useful for error reporting.
	LL_INLINE S32 getLineNumber()					{ return mLineNumber; }

	// Returns required size of output buffer
	U32 getOutputSize();

	// Writes contents to datapacker
	bool serialize(LLDataPacker& dp);

	// Flags redundant keyframe data
	void optimize();

	void reset();

	LL_INLINE F32 getDuration()						{ return mDuration; }

	LL_INLINE bool isInitialized()					{ return mInitialized; }

	LL_INLINE ELoadStatus getStatus()				{ return mStatus; }

protected:
	// Consumes one line of input from file.
	bool getLine(LLFILE* fp);

protected:
	// Parsed values
	JointVector			mJoints;
	ConstraintVector	mConstraints;
	TranslationMap		mTranslations;
	S32					mNumFrames;
	F32					mFrameTime;

	S32					mPriority;
	F32					mLoopInPoint;
	F32					mLoopOutPoint;
	F32					mEaseIn;
	F32					mEaseOut;
	S32					mHand;
	std::string			mEmoteName;

	ELoadStatus			mStatus;
	// Computed values
	F32					mDuration;

	// Parser state
	S32					mLineNumber;
	char				mLine[BVH_PARSER_LINE_SIZE];

	bool				mInitialized;
	bool				mLoop;
};

#endif // LL_LLBVHLOADER_H
