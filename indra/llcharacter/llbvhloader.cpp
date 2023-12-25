/**
 * @file llbvhloader.cpp
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

#include "linden_common.h"

#include <deque>

#include "boost/lexical_cast.hpp"
#include "boost/tokenizer.hpp"

#include "llbvhloader.h"

#include "lldatapacker.h"
#include "lldir.h"
#include "llkeyframemotion.h"
#include "llquantize.h"
#include "llsdserialize.h"
#include "llstl.h"

using namespace std;

// The .bvh does not have a formal spec, and different readers interpret things
// in their own way. In OUR usage, frame 0 is used in optimization and is not
// considered to be part of the animation.
constexpr S32 NUMBER_OF_IGNORED_FRAMES_AT_START = 1;
// In our usage, the last frame is used only to indicate what the penultimate
// frame should be interpolated towards. I.e. the animation only plays up to
// the start of the last frame. There is no hold or exptrapolation past that
// point. Thus there are two frame of the total that do not contribute to the
// total running time of the animation.
constexpr S32 NUMBER_OF_UNPLAYED_FRAMES = 2;

constexpr F32 POSITION_KEYFRAME_THRESHOLD_SQUARED = 0.03f * 0.03f;
constexpr F32 ROTATION_KEYFRAME_THRESHOLD = 0.01f;

constexpr F32 POSITION_MOTION_THRESHOLD_SQUARED = 0.001f * 0.001f;
constexpr F32 ROTATION_MOTION_THRESHOLD = 0.001f;

char gInFile[1024];
char gOutFile[1024];

const char* find_next_whitespace(const char* p)
{
	while (*p && isspace(*p)) ++p;
	while (*p && !isspace(*p)) ++p;
	return p;
}

// XYZ order in BVH files must be passed to mayaQ() as ZYX. This function
// reverses the input string before passing it on to StringToOrder().
LLQuaternion::Order bvh_str_to_order(char* str)
{
	char order[4];
	order[0] = str[2];
	order[1] = str[1];
	order[2] = str[0];
	order[3] = '\0';
	LLQuaternion::Order ret = StringToOrder(order);
	return ret;
}

LLBVHLoader::LLBVHLoader(const char* buffer, ELoadStatus& load_status,
						 S32& error_line,
						 std::map<std::string, std::string>& joint_alias_map)
{
	reset();
	error_line = 0;
	mStatus = loadTranslationTable("anim.ini");
	load_status = mStatus;
	llinfos << "Load Status 00 : "<< load_status << llendl;
	if (mStatus == E_ST_NO_XLT_FILE)
	{
		LL_DEBUGS("BVHLoader") << "No translation table found." << LL_ENDL;
		load_status = mStatus;
		return;
	}
	else if (mStatus != E_ST_OK)
	{
		LL_DEBUGS("BVHLoader") << "ERROR: [line: " << getLineNumber() << "] "
							   << mStatus << LL_ENDL;
		error_line = getLineNumber();
		load_status = mStatus;
		return;
	}

	// Recognize all names we have been told are legal.
	for (std::map<std::string, std::string>::iterator
			iter = joint_alias_map.begin(), end = joint_alias_map.end();
		 iter != end; ++iter)
	{
		makeTranslation(iter->first , iter->second);
	}

	char error_text[128];
	// Read all joints in BVH file.
	mStatus = loadBVHFile(buffer, error_text, error_line);
	LL_DEBUGS("BVHLoader") << "Raw data from file:";
	dumpBVHInfo();
	LL_CONT << LL_ENDL;

	if (mStatus != E_ST_OK)
	{
		LL_DEBUGS("BVHLoader") << "ERROR: [line: " << getLineNumber() << "] "
							   << mStatus << LL_ENDL;
		load_status = mStatus;
		error_line = getLineNumber();
		return;
	}

	// Maps between joints found in file and the aliased names.
	applyTranslations();
	optimize();

	LL_DEBUGS("BVHLoader") << "After translations and optimize:";
	dumpBVHInfo();
	LL_CONT << LL_ENDL;

	mInitialized = true;
	error_line = 0;
}

LLBVHLoader::~LLBVHLoader()
{
	std::for_each(mJoints.begin(), mJoints.end(), DeletePointer());
	mJoints.clear();
}

ELoadStatus LLBVHLoader::loadTranslationTable(const char* filename)
{
	//--------------------------------------------------------------------
	// Open file
	//--------------------------------------------------------------------
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
													  filename);

	LLFile infile(path, "r");
	if (!infile)
	{
		return E_ST_NO_XLT_FILE;
	}

	llinfos << "Loading translation table: " << filename << llendl;

	//--------------------------------------------------------------------
	// Register file to be closed on function exit
	//--------------------------------------------------------------------

	//--------------------------------------------------------------------
	// Load header
	//--------------------------------------------------------------------
	if (!getLine(infile))
	{
		return E_ST_EOF;
	}
	if (strncmp(mLine, "Translations 1.0", 16))
	{
		return E_ST_NO_XLT_HEADER;
	}

	//--------------------------------------------------------------------
	// load data one line at a time
	//--------------------------------------------------------------------
	bool loadingGlobals = false;
	while (getLine(infile))
	{
		//----------------------------------------------------------------
		// Check the 1st token on the line to determine if it's empty or a
		// comment
		//----------------------------------------------------------------
		char token[128];
		if (sscanf(mLine, " %127s", token) != 1)
		{
			continue;
		}

		if (token[0] == '#')
		{
			continue;
		}

		//----------------------------------------------------------------
		// Check if a [jointName] or [GLOBALS] was specified.
		//----------------------------------------------------------------
		if (token[0] == '[')
		{
			char name[128];
			if (sscanf(mLine, " [%127[^]]", name) != 1)
			{
				return E_ST_NO_XLT_NAME;
			}

			if (strcmp(name, "GLOBALS") == 0)
			{
				loadingGlobals = true;
				continue;
			}
		}

		//----------------------------------------------------------------
		// Check for optional emote
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "emote") == 0)
		{
			char emote_str[1024];
			if (sscanf(mLine, " %*s = %1023s", emote_str) != 1)
			{
				return E_ST_NO_XLT_EMOTE;
			}
			mEmoteName.assign(emote_str);
			LL_DEBUGS("BVHLoader") << "Emote: " << mEmoteName.c_str()
								   << LL_ENDL;
			continue;
		}

		//----------------------------------------------------------------
		// Check for global priority setting
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "priority") == 0)
		{
			S32 priority;
			if (sscanf(mLine, " %*s = %d", &priority) != 1)
			{
				return E_ST_NO_XLT_PRIORITY;
			}
			mPriority = priority;
			LL_DEBUGS("BVHLoader") << "Priority: " << mPriority << LL_ENDL;
			continue;
		}

		//----------------------------------------------------------------
		// Check for global loop setting
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "loop") == 0)
		{
			char trueFalse[128];
			trueFalse[0] = '\0';

			F32 loop_in = 0.f;
			F32 loop_out = 1.f;

			if (sscanf(mLine, " %*s = %f %f", &loop_in, &loop_out) == 2)
			{
				mLoop = true;
			}
			else if (sscanf(mLine, " %*s = %127s", trueFalse) == 1)
			{
				mLoop = LLStringUtil::compareInsensitive(trueFalse, "true") == 0;
			}
			else
			{
				return E_ST_NO_XLT_LOOP;
			}

			mLoopInPoint = loop_in * mDuration;
			mLoopOutPoint = loop_out * mDuration;

			continue;
		}

		//----------------------------------------------------------------
		// Check for global easeIn setting
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "easein") == 0)
		{
			F32 duration;
			char type[128];
			if (sscanf(mLine, " %*s = %f %127s", &duration, type) != 2)
			{
				return E_ST_NO_XLT_EASEIN;
			}
			mEaseIn = duration;
			continue;
		}

		//----------------------------------------------------------------
		// Check for global easeOut setting
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "easeout") == 0)
		{
			F32 duration;
			char type[128];
			if (sscanf(mLine, " %*s = %f %127s", &duration, type) != 2)
			{
				return E_ST_NO_XLT_EASEOUT;
			}
			mEaseOut = duration;
			continue;
		}

		//----------------------------------------------------------------
		// Check for global handMorph setting
		//----------------------------------------------------------------
		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "hand") == 0)
		{
			S32 handMorph;
			if (sscanf(mLine, " %*s = %d", &handMorph) != 1)
			{
				return E_ST_NO_XLT_HAND;
			}
			mHand = handMorph;
			continue;
		}

		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "constraint") == 0)
		{
			Constraint constraint;

			// Try reading optional target direction
			if (sscanf(mLine,
					   " %*s = %d %f %f %f %f %15s %f %f %f %15s %f %f %f %f %f %f",
					   &constraint.mChainLength,
					   &constraint.mEaseInStart,
					   &constraint.mEaseInStop,
					   &constraint.mEaseOutStart,
					   &constraint.mEaseOutStop,
					   constraint.mSourceJointName,
					   &constraint.mSourceOffset.mV[VX],
					   &constraint.mSourceOffset.mV[VY],
					   &constraint.mSourceOffset.mV[VZ],
					   constraint.mTargetJointName,
					   &constraint.mTargetOffset.mV[VX],
					   &constraint.mTargetOffset.mV[VY],
					   &constraint.mTargetOffset.mV[VZ],
					   &constraint.mTargetDir.mV[VX],
					   &constraint.mTargetDir.mV[VY],
					   &constraint.mTargetDir.mV[VZ]) != 16)
			{
				if (sscanf(mLine,
					" %*s = %d %f %f %f %f %15s %f %f %f %15s %f %f %f",
					&constraint.mChainLength,
					&constraint.mEaseInStart,
					&constraint.mEaseInStop,
					&constraint.mEaseOutStart,
					&constraint.mEaseOutStop,
					constraint.mSourceJointName,
					&constraint.mSourceOffset.mV[VX],
					&constraint.mSourceOffset.mV[VY],
					&constraint.mSourceOffset.mV[VZ],
					constraint.mTargetJointName,
					&constraint.mTargetOffset.mV[VX],
					&constraint.mTargetOffset.mV[VY],
					&constraint.mTargetOffset.mV[VZ]) != 13)
				{
					return E_ST_NO_CONSTRAINT;
				}
			}
			else if (!constraint.mTargetDir.isExactlyZero())
			{
				// Normalize direction
				constraint.mTargetDir.normalize();
			}

			constraint.mConstraintType = CONSTRAINT_TYPE_POINT;
			mConstraints.push_back(constraint);
			continue;
		}

		if (loadingGlobals &&
			LLStringUtil::compareInsensitive(token, "planar_constraint") == 0)
		{
			Constraint constraint;

			// Try reading optional target direction
			if (sscanf(mLine,
				" %*s = %d %f %f %f %f %15s %f %f %f %15s %f %f %f %f %f %f",
				&constraint.mChainLength,
				&constraint.mEaseInStart,
				&constraint.mEaseInStop,
				&constraint.mEaseOutStart,
				&constraint.mEaseOutStop,
				constraint.mSourceJointName,
				&constraint.mSourceOffset.mV[VX],
				&constraint.mSourceOffset.mV[VY],
				&constraint.mSourceOffset.mV[VZ],
				constraint.mTargetJointName,
				&constraint.mTargetOffset.mV[VX],
				&constraint.mTargetOffset.mV[VY],
				&constraint.mTargetOffset.mV[VZ],
				&constraint.mTargetDir.mV[VX],
				&constraint.mTargetDir.mV[VY],
				&constraint.mTargetDir.mV[VZ]) != 16)
			{
				if (sscanf(mLine,
						   " %*s = %d %f %f %f %f %15s %f %f %f %15s %f %f %f",
						   &constraint.mChainLength,
						   &constraint.mEaseInStart,
						   &constraint.mEaseInStop,
						   &constraint.mEaseOutStart,
						   &constraint.mEaseOutStop,
						   constraint.mSourceJointName,
						   &constraint.mSourceOffset.mV[VX],
						   &constraint.mSourceOffset.mV[VY],
						   &constraint.mSourceOffset.mV[VZ],
						   constraint.mTargetJointName,
						   &constraint.mTargetOffset.mV[VX],
						   &constraint.mTargetOffset.mV[VY],
						   &constraint.mTargetOffset.mV[VZ]) != 13)
				{
					return E_ST_NO_CONSTRAINT;
				}
			}
			else if (!constraint.mTargetDir.isExactlyZero())
			{
				// Normalize direction
				constraint.mTargetDir.normalize();
			}

			constraint.mConstraintType = CONSTRAINT_TYPE_PLANE;
			mConstraints.push_back(constraint);
		}
	}

	return E_ST_OK;
}

void LLBVHLoader::makeTranslation(const std::string& alias_name,
								  const std::string& joint_name)
{
	// This uses []'s implicit call to ctor.
	Translation& new_trans = mTranslations[alias_name];

	new_trans.mOutName = joint_name;
	if (joint_name == "mPelvis")
	{
		new_trans.mRelativePositionKey = true;
		new_trans.mRelativeRotationKey = true;
	}

	LLMatrix3 fm;
	LLVector3 vect1(0.f, 1.f, 0.f);
	LLVector3 vect2(0.f, 0.f, 1.f);	
	LLVector3 vect3(1.f, 0.f, 0.f);
	fm.setRows(vect1, vect2, vect3);
	new_trans.mFrameMatrix = fm;
}

#if 0	// Not used
ELoadStatus LLBVHLoader::loadAliases(const char* filename)
{
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
													  filename);
	llifstream input_stream;
	input_stream.open(path.c_str(), std::ios::in | std::ios::binary);
	if (!input_stream.is_open())
	{
		llwarns << "Cannot open joint alias file " << path << llendl;
		return E_ST_NO_XLT_FILE;
	}

	LLSD aliases_sd;
	if (LLSDSerialize::fromXML(aliases_sd, input_stream))
	{
		for (LLSD::map_iterator it = aliases_sd.beginMap(),
								end = aliases_sd.endMap();
			 it != end; ++it)
		{
			LLSD::String alias_name = it->first;
			LLSD::String joint_name = it->second;
			makeTranslation(alias_name, joint_name);
		}
	}
	else
	{
		input_stream.close();
		return E_ST_NO_XLT_HEADER;
	}
	input_stream.close();

	return E_ST_OK;
}
#endif

void LLBVHLoader::dumpBVHInfo()
{
	for (U32 j = 0, count = mJoints.size(); j < count; ++j)
	{
		Joint* joint = mJoints[j];
		LL_DEBUGS("BVHLoader") << "Joint: " << joint->mName << LL_ENDL;
		for (S32 i = 0, count = llmin(mNumFrames, (S32)joint->mKeys.size());
			 i < count; ++i)
		{
			Key& prevkey = joint->mKeys[llmax(i - 1, 0)];
			Key& key = joint->mKeys[i];
			if (i == 0 ||
				key.mPos[0] != prevkey.mPos[0] ||
				key.mPos[1] != prevkey.mPos[1] ||
				key.mPos[2] != prevkey.mPos[2] ||
				key.mRot[0] != prevkey.mRot[0] ||
				key.mRot[1] != prevkey.mRot[1] ||
				key.mRot[2] != prevkey.mRot[2])
			{
				LL_DEBUGS("BVHLoader") << "  Frame: " << i << " - Pos: "
									   << key.mPos[0] << "," << key.mPos[1]
									   << "," << key.mPos[2] << " - Rot: "
									   << key.mRot[0] << "," << key.mRot[1]
									   << "," << key.mRot[2] << LL_ENDL;
			}
		}
	}
}

ELoadStatus LLBVHLoader::loadBVHFile(const char* buffer, char* error_text,
									 S32& err_line)
{
	std::string line;

	err_line = 0;
	error_text[127] = '\0';

	std::string str(buffer);
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\r\n");
	tokenizer tokens(str, sep);
	tokenizer::iterator iter = tokens.begin();

	mLineNumber = 0;
	mJoints.clear();

	std::vector<S32> parent_joints;

	//--------------------------------------------------------------------
	// Consume  hierarchy
	//--------------------------------------------------------------------
	if (iter == tokens.end())
	{
		return E_ST_EOF;
	}
	line = *(iter++);
	++err_line;

	if (!strstr(line.c_str(), "HIERARCHY"))
	{
		return E_ST_NO_HIER;
	}

	//--------------------------------------------------------------------
	// Consume joints
	//--------------------------------------------------------------------
	while (true)
	{
		//----------------------------------------------------------------
		// Get next line
		//----------------------------------------------------------------
		if (iter == tokens.end())
		{
			return E_ST_EOF;
		}
		line = *(iter++);
		++err_line;

		//----------------------------------------------------------------
		// Consume }
		//----------------------------------------------------------------
		if (strstr(line.c_str(), "}"))
		{
			if (parent_joints.size() > 0)
			{
				parent_joints.pop_back();
			}
			continue;
		}

		//----------------------------------------------------------------
		// If MOTION, break out
		//----------------------------------------------------------------
		if (strstr(line.c_str(), "MOTION"))
			break;

		//----------------------------------------------------------------
		// It must be either ROOT or JOINT or EndSite
		//----------------------------------------------------------------
		if (strstr(line.c_str(), "ROOT") || strstr(line.c_str(), "JOINT"))
		{
		}
		else if (strstr(line.c_str(), "End Site"))
		{
			++iter; // {
			++iter; //     OFFSET
			++iter; // }
			S32 depth = 0;
			for (S32 j = (S32)parent_joints.size() - 1; j >= 0; --j)
			{
				Joint* joint = mJoints[parent_joints[j]];
				if (depth > joint->mChildTreeMaxDepth)
				{
					joint->mChildTreeMaxDepth = depth;
				}
				++depth;
			}
			continue;
		}
		else
		{
			strncpy(error_text, line.c_str(), 127);
			return E_ST_NO_JOINT;
		}

		//----------------------------------------------------------------
		// Get the joint name
		//----------------------------------------------------------------
		char jointName[80];
		if (sscanf(line.c_str(), "%*s %79s", jointName) != 1)
		{
			strncpy(error_text, line.c_str(), 127);
			return E_ST_NO_NAME;
		}

		//---------------------------------------------------------------
		// We require the root joint be "hip" - DEV-26188
		//---------------------------------------------------------------
		if (mJoints.size() == 0)
		{
			// The root joint of the BVH file must be hip (mPelvis) or an alias
			// of mPelvis.
			const char* FORCED_ROOT_NAME = "hip";
			TranslationMap::iterator hip_joint = mTranslations.find(FORCED_ROOT_NAME);
			TranslationMap::iterator root_joint = mTranslations.find(jointName);
			if (hip_joint == mTranslations.end() ||
				root_joint == mTranslations.end() ||
				root_joint->second.mOutName != hip_joint->second.mOutName)
			{
				strncpy(error_text, line.c_str(), 127);
				return E_ST_BAD_ROOT;
 			}
		}

		//----------------------------------------------------------------
		// Add a set of keyframes for this joint
		//----------------------------------------------------------------
		mJoints.push_back(new Joint(jointName));
		Joint* joint = mJoints.back();
		LL_DEBUGS("BVHLoader") << "Created joint: " << jointName
							   << " - Index: " << mJoints.size() - 1
							   << LL_ENDL;

		S32 depth = 1;
		for (S32 j = (S32)parent_joints.size() - 1; j >= 0; --j)
		{
			Joint* pjoint = mJoints[parent_joints[j]];
			LL_DEBUGS("BVHLoader") << "Ancestor: " << pjoint->mName << LL_ENDL;
			if (depth > pjoint->mChildTreeMaxDepth)
			{
				pjoint->mChildTreeMaxDepth = depth;
			}
			++depth;
		}

		//----------------------------------------------------------------
		// Get next line
		//----------------------------------------------------------------
		if (iter == tokens.end())
		{
			return E_ST_EOF;
		}
		line = *(iter++);
		++err_line;

		//----------------------------------------------------------------
		// It must be {
		//----------------------------------------------------------------
		if (!strstr(line.c_str(), "{"))
		{
			strncpy(error_text, line.c_str(), 127);
			return E_ST_NO_OFFSET;
		}
		else
		{
			parent_joints.push_back((S32)mJoints.size() - 1);
		}

		//----------------------------------------------------------------
		// Get next line
		//----------------------------------------------------------------
		if (iter == tokens.end())
		{
			return E_ST_EOF;
		}
		line = *(iter++);
		++err_line;

		//----------------------------------------------------------------
		// It must be OFFSET
		//----------------------------------------------------------------
		if (!strstr(line.c_str(), "OFFSET"))
		{
			strncpy(error_text, line.c_str(), 127);
			return E_ST_NO_OFFSET;
		}

		//----------------------------------------------------------------
		// Get next line
		//----------------------------------------------------------------
		if (iter == tokens.end())
		{
			return E_ST_EOF;
		}
		line = *(iter++);
		++err_line;

		//----------------------------------------------------------------
		// It must be CHANNELS
		//----------------------------------------------------------------
		if (!strstr(line.c_str(), "CHANNELS"))
		{
			strncpy(error_text, line.c_str(), 127);
			return E_ST_NO_CHANNELS;
		}

		// Animating position (via mNumChannels = 6) is only supported for
		// mPelvis.
		if (sscanf(line.c_str(), " CHANNELS %d", &joint->mNumChannels) != 1)
		{
			// Assume default if not otherwise specified.
			if (mJoints.size() == 1)
			{
				joint->mNumChannels = 6;
			}
			else
			{
				joint->mNumChannels = 3;
			}
		}

		//----------------------------------------------------------------
		// Get rotation order
		//----------------------------------------------------------------
		const char* p = line.c_str();
		for (S32 i = 0; i < 3; ++i)
		{
			p = strstr(p, "rotation");
			if (!p)
			{
				strncpy(error_text, line.c_str(), 127);
				return E_ST_NO_ROTATION;
			}

			const char axis = *(p - 1);
			if (axis != 'X' && axis != 'Y' && axis != 'Z')
			{
				strncpy(error_text, line.c_str(), 127);
				return E_ST_NO_AXIS;
			}

			joint->mOrder[i] = axis;

			++p;
		}
	}

	//--------------------------------------------------------------------
	// Consume motion
	//--------------------------------------------------------------------
	if (!strstr(line.c_str(), "MOTION"))
	{
		strncpy(error_text, line.c_str(), 127);
		return E_ST_NO_MOTION;
	}

	//--------------------------------------------------------------------
	// Get number of frames
	//--------------------------------------------------------------------
	if (iter == tokens.end())
	{
		return E_ST_EOF;
	}
	line = *(iter++);
	++err_line;

	if (!strstr(line.c_str(), "Frames:"))
	{
		strncpy(error_text, line.c_str(), 127);
		return E_ST_NO_FRAMES;
	}

	if (sscanf(line.c_str(), "Frames: %d", &mNumFrames) != 1)
	{
		strncpy(error_text, line.c_str(), 127);
		return E_ST_NO_FRAMES;
	}

	//--------------------------------------------------------------------
	// Get frame time
	//--------------------------------------------------------------------
	if (iter == tokens.end())
	{
		return E_ST_EOF;
	}
	line = *(iter++);
	++err_line;

	if (!strstr(line.c_str(), "Frame Time:"))
	{
		strncpy(error_text, line.c_str(), 127);
		return E_ST_NO_FRAME_TIME;
	}

	if (sscanf(line.c_str(), "Frame Time: %f", &mFrameTime) != 1)
	{
		strncpy(error_text, line.c_str(), 127);
		return E_ST_NO_FRAME_TIME;
	}

	if (mNumFrames > NUMBER_OF_UNPLAYED_FRAMES)
	{
		mDuration = (F32)(mNumFrames - NUMBER_OF_UNPLAYED_FRAMES) * mFrameTime;
	}
	else
	{
		// If the user only supplies one animation frame (after the ignored
		// reference frame 0), hold for mFrameTime.
		mDuration = (F32)mNumFrames * mFrameTime;
	}
	if (!mLoop)
	{
		mLoopOutPoint = mDuration;
	}

	//--------------------------------------------------------------------
	// Load frames
	//--------------------------------------------------------------------
	for (S32 i = 0; i < mNumFrames; ++i)
	{
		// get next line
		if (iter == tokens.end())
		{
			return E_ST_EOF;
		}
		line = *(iter++);
		++err_line;

		// Split line into a collection of floats.
		std::deque<F32> floats;
		boost::char_separator<char> whitespace_sep("\t ");
		tokenizer float_tokens(line, whitespace_sep);
		tokenizer::iterator float_token_iter = float_tokens.begin();
		while (float_token_iter != float_tokens.end())
		{
			try
			{
				F32 val = boost::lexical_cast<float>(*float_token_iter);
				floats.push_back(val);
			}
			catch (const boost::bad_lexical_cast&)
			{
				strncpy(error_text, line.c_str(), 127);
				return E_ST_NO_POS;
			}
			++float_token_iter;
		}
		LL_DEBUGS("BVHLoader") << "Got " << floats.size() << " floats."
							   << LL_ENDL;

		for (U32 j = 0; j < mJoints.size(); ++j)
		{
			Joint* joint = mJoints[j];
			joint->mKeys.push_back(Key());
			Key& key = joint->mKeys.back();

			if (floats.size() < (size_t)joint->mNumChannels)
			{
				strncpy(error_text, line.c_str(), 127);
				return E_ST_NO_ROT;
			}

			// assume either numChannels == 6, in which case we have pos + rot,
			// or numChannels == 3, in which case we have only rot.
			if (joint->mNumChannels == 6)
			{
				key.mPos[0] = floats.front();
				floats.pop_front();
				key.mPos[1] = floats.front();
				floats.pop_front();
				key.mPos[2] = floats.front();
				floats.pop_front();
			}

			key.mRot[joint->mOrder[0] - 'X'] = floats.front();
			floats.pop_front();
			key.mRot[joint->mOrder[1] - 'X'] = floats.front();
			floats.pop_front();
			key.mRot[joint->mOrder[2] - 'X'] = floats.front();
			floats.pop_front();
		}
	}

	return E_ST_OK;
}

void LLBVHLoader::applyTranslations()
{
	for (JointVector::iterator ji = mJoints.begin(), end = mJoints.end();
		 ji != end; ++ji)
	{
		Joint* joint = *ji;
		//----------------------------------------------------------------
		// Look for a translation for this joint.
		// If none, skip to next joint
		//----------------------------------------------------------------
		TranslationMap::iterator ti = mTranslations.find(joint->mName);
		if (ti == mTranslations.end())
		{
			continue;
		}

		Translation &trans = ti->second;

		//----------------------------------------------------------------
		// Set the ignore flag if necessary
		//----------------------------------------------------------------
		if (trans.mIgnore)
		{
			LL_DEBUGS("BVHLoader") << "Ignoring " << joint->mName.c_str()
								   << LL_ENDL;
			joint->mIgnore = true;
			continue;
		}

		//----------------------------------------------------------------
		// Set the output name
		//----------------------------------------------------------------
		if (!trans.mOutName.empty())
		{
			LL_DEBUGS("BVHLoader") << "Changing " << joint->mName.c_str()
								   << " to " << trans.mOutName.c_str()
								   << LL_ENDL;
			joint->mOutName = trans.mOutName;
		}

		// Allow joint position changes as of SL-318, excepted for 3 channels
		// animations.
		joint->mIgnorePositions = joint->mNumChannels == 3;

		//----------------------------------------------------------------
		// Set the relativepos flags if necessary
		//----------------------------------------------------------------
		if (trans.mRelativePositionKey)
		{
			LL_DEBUGS("BVHLoader") << "Removing 1st position offset from all keys for "
								   << joint->mOutName.c_str() << LL_ENDL;
			joint->mRelativePositionKey = true;
		}

		if (trans.mRelativeRotationKey)
		{
			LL_DEBUGS("BVHLoader") << "Removing 1st rotation from all keys for "
								   << joint->mOutName.c_str() << LL_ENDL;
			joint->mRelativeRotationKey = true;
		}

		if (trans.mRelativePosition.length() > 0.0f)
		{
			joint->mRelativePosition = trans.mRelativePosition;
			LL_DEBUGS("BVHLoader") << "Removing "
								   << joint->mRelativePosition.mV[0] << " "
								   << joint->mRelativePosition.mV[1] << " "
								   << joint->mRelativePosition.mV[2]
								   << " from all position keys in "
								   << joint->mOutName.c_str() << LL_ENDL;
		}

		//----------------------------------------------------------------
		// Set change of coordinate frame
		//----------------------------------------------------------------
		joint->mFrameMatrix = trans.mFrameMatrix;
		joint->mOffsetMatrix = trans.mOffsetMatrix;

		//----------------------------------------------------------------
		// Set mergeparent name
		//----------------------------------------------------------------
		if (!trans.mMergeParentName.empty())
		{
			LL_DEBUGS("BVHLoader") << "Merging " << joint->mOutName.c_str()
								   << " with parent "
								   << trans.mMergeParentName.c_str()
								   << LL_ENDL;
			joint->mMergeParentName = trans.mMergeParentName;
		}

		//----------------------------------------------------------------
		// Set mergechild name
		//----------------------------------------------------------------
		if (!trans.mMergeChildName.empty())
		{
			LL_DEBUGS("BVHLoader") << "Merging " << joint->mName.c_str()
								   << " with child "
								   << trans.mMergeChildName.c_str() << LL_ENDL;
			joint->mMergeChildName = trans.mMergeChildName;
		}

		//----------------------------------------------------------------
		// Set joint priority
		//----------------------------------------------------------------
		joint->mPriority = mPriority + trans.mPriorityModifier;
	}
}

void LLBVHLoader::optimize()
{
	// RN: assume motion blend, which is the default now
	if (!mLoop && mEaseIn + mEaseOut > mDuration && mDuration != 0.f)
	{
		F32 factor = mDuration / (mEaseIn + mEaseOut);
		mEaseIn *= factor;
		mEaseOut *= factor;
	}

	for (JointVector::iterator ji = mJoints.begin(), end = mJoints.end();
		 ji != end; ++ji)
	{
		Joint* joint = *ji;
		bool pos_changed = false;
		bool rot_changed = false;

		if (!joint->mIgnore)
		{
			joint->mNumPosKeys = 0;
			joint->mNumRotKeys = 0;
			LLQuaternion::Order order = bvh_str_to_order(joint->mOrder);

			KeyVector::iterator first_key = joint->mKeys.begin();

			// No key ?
			if (first_key == joint->mKeys.end())
			{
				joint->mIgnore = true;
				continue;
			}

			LLVector3 first_frame_pos(first_key->mPos);
			LLQuaternion first_frame_rot = mayaQ(first_key->mRot[0],
												 first_key->mRot[1],
												 first_key->mRot[2], order);

			// Skip first key
			KeyVector::iterator ki = joint->mKeys.begin();
			if (joint->mKeys.size() == 1)
			{
				// *FIX: use single frame to move pelvis
				// if only one keyframe force output for this joint
				rot_changed = true;
			}
			else
			{
				// If more than one keyframe, use first frame as reference and
				// skip to second
				first_key->mIgnorePos = true;
				first_key->mIgnoreRot = true;
				++ki;
			}

			KeyVector::iterator ki_prev = ki;
			KeyVector::iterator ki_last_good_pos = ki;
			KeyVector::iterator ki_last_good_rot = ki;
			S32 numPosFramesConsidered = 2;
			S32 numRotFramesConsidered = 2;

			F32 rot_threshold = ROTATION_KEYFRAME_THRESHOLD /
								llmax((F32)joint->mChildTreeMaxDepth * 0.33f,
									   1.f);

			double diff_max = 0;
			KeyVector::iterator ki_max = ki;
			for ( ; ki != joint->mKeys.end(); ++ki)
			{
				if (ki_prev == ki_last_good_pos)
				{
					++joint->mNumPosKeys;
					if (dist_vec_squared(LLVector3(ki_prev->mPos),
										 first_frame_pos) > POSITION_MOTION_THRESHOLD_SQUARED)
					{
						pos_changed = true;
					}
				}
				else
				{
					// Check position for noticeable effect
					LLVector3 test_pos(ki_prev->mPos);
					LLVector3 last_good_pos(ki_last_good_pos->mPos);
					LLVector3 current_pos(ki->mPos);
					LLVector3 interp_pos = lerp(current_pos,
												last_good_pos,
												1.f / (F32)numPosFramesConsidered);

					if (dist_vec_squared(current_pos,
										 first_frame_pos) > POSITION_MOTION_THRESHOLD_SQUARED)
					{
						pos_changed = true;
					}

					if (dist_vec_squared(interp_pos,
										 test_pos) < POSITION_KEYFRAME_THRESHOLD_SQUARED)
					{
						ki_prev->mIgnorePos = true;
						++numPosFramesConsidered;
					}
					else
					{
						numPosFramesConsidered = 2;
						ki_last_good_pos = ki_prev;
						++joint->mNumPosKeys;
					}
				}

				if (ki_prev == ki_last_good_rot)
				{
					++joint->mNumRotKeys;
					LLQuaternion test_rot = mayaQ(ki_prev->mRot[0],
												  ki_prev->mRot[1],
												  ki_prev->mRot[2], order);
					F32 x_delta = dist_vec(LLVector3::x_axis * first_frame_rot,
										   LLVector3::x_axis * test_rot);
					F32 y_delta = dist_vec(LLVector3::y_axis * first_frame_rot,
										   LLVector3::y_axis * test_rot);
					F32 rot_test = x_delta + y_delta;

					if (rot_test > ROTATION_MOTION_THRESHOLD)
					{
						rot_changed = true;
					}
				}
				else
				{
					// Check rotation for noticeable effect
					LLQuaternion test_rot = mayaQ(ki_prev->mRot[0],
												  ki_prev->mRot[1],
												  ki_prev->mRot[2], order);
					LLQuaternion last_good_rot = mayaQ(ki_last_good_rot->mRot[0],
													   ki_last_good_rot->mRot[1],
													   ki_last_good_rot->mRot[2],
													   order);
					LLQuaternion current_rot = mayaQ(ki->mRot[0],
													 ki->mRot[1],
													 ki->mRot[2], order);
					LLQuaternion interp_rot = lerp(1.f / (F32)numRotFramesConsidered,
												   current_rot, last_good_rot);

					F32 x_delta;
					F32 y_delta;
					F32 rot_test;

					// Test if the rotation has changed significantly since the
					// very first frame. If false for all frames, then we'll
					// just throw out this joint's rotation entirely.
					x_delta = dist_vec(LLVector3::x_axis * first_frame_rot,
									   LLVector3::x_axis * test_rot);
					y_delta = dist_vec(LLVector3::y_axis * first_frame_rot,
									   LLVector3::y_axis * test_rot);
					rot_test = x_delta + y_delta;
					if (rot_test > ROTATION_MOTION_THRESHOLD)
					{
						rot_changed = true;
					}
					x_delta = dist_vec(LLVector3::x_axis * interp_rot,
									   LLVector3::x_axis * test_rot);
					y_delta = dist_vec(LLVector3::y_axis * interp_rot,
									   LLVector3::y_axis * test_rot);
					rot_test = x_delta + y_delta;

					// Draw a line between the last good keyframe and current.
					// Test the distance between the last frame (current-1,
					// i.e. ki_prev) and the line. If it's greater than some
					// threshold, then it represents a significant frame and we
					// want to include it.
					if (rot_test >= rot_threshold ||
						(ki + 1 == joint->mKeys.end() &&
						 numRotFramesConsidered > 2))
					{
						// Add the current test keyframe (which is technically
						// the previous key, i.e. ki_prev).
						numRotFramesConsidered = 2;
						ki_last_good_rot = ki_prev;
						++joint->mNumRotKeys;

						// Add another keyframe between the last good keyframe
						// and current, at whatever point was the most
						// "significant" (i.e. had the largest deviation from
						// the earlier tests). Note that a more robust approach
						// would be test all intermediate keyframes against the
						// line between the last good keyframe and current, but
						// we're settling for this other method because it's
						// significantly faster.
						if (diff_max > 0)
						{
							if (ki_max->mIgnoreRot)
							{
								ki_max->mIgnoreRot = false;
								++joint->mNumRotKeys;
							}
							diff_max = 0;
						}
					}
					else
					{
						// This keyframe isn't significant enough, throw it
						// away.
						ki_prev->mIgnoreRot = true;
						++numRotFramesConsidered;
						// Store away the keyframe that has the largest
						// deviation from the interpolated line, for insertion
						// later.
						if (rot_test > diff_max)
						{
							diff_max = rot_test;
							ki_max = ki;
						}
					}
				}

				ki_prev = ki;
			}
		}

		// Do not output joints with no motion
		if (!(pos_changed || rot_changed))
		{
			LL_DEBUGS("BVHLoader") << "Ignoring joint " << joint->mName
								   << LL_ENDL;
			joint->mIgnore = true;
		}
	}
}

void LLBVHLoader::reset()
{
	mLineNumber = 0;
	mNumFrames = 0;
	mFrameTime = 0.0f;
	mDuration = 0.0f;

	mPriority = 2;
	mLoop = false;
	mLoopInPoint = 0.f;
	mLoopOutPoint = 0.f;
	mEaseIn = 0.3f;
	mEaseOut = 0.3f;
	mHand = 1;
	mInitialized = false;

	mEmoteName.clear();
	mLineNumber = 0;
	mTranslations.clear();
	mConstraints.clear();
}

bool LLBVHLoader::getLine(LLFILE* fp)
{
	if (!feof(fp) && fgets(mLine, BVH_PARSER_LINE_SIZE, fp))
	{
		++mLineNumber;
		return true;
	}
	return false;
}

// Returns required size of output buffer
U32 LLBVHLoader::getOutputSize()
{
	LLDataPackerBinaryBuffer dp;
	serialize(dp);
	return dp.getCurrentSize();
}

// Writes contents to datapacker
bool LLBVHLoader::serialize(LLDataPacker& dp)
{
	// Count number of non-ignored joints
	S32 num_joints = 0;
	for (JointVector::iterator ji = mJoints.begin(), end = mJoints.end();
		 ji != end; ++ji)
	{
		Joint* joint = *ji;
		if (joint && !joint->mIgnore)
		{
			++num_joints;
		}
	}

	// Print header
	dp.packU16(KEYFRAME_MOTION_VERSION, "version");
	dp.packU16(KEYFRAME_MOTION_SUBVERSION, "sub_version");
	dp.packS32(mPriority, "base_priority");
	dp.packF32(mDuration, "duration");
	dp.packString(mEmoteName, "emote_name");
	dp.packF32(mLoopInPoint, "loop_in_point");
	dp.packF32(mLoopOutPoint, "loop_out_point");
	dp.packS32(mLoop, "loop");
	dp.packF32(mEaseIn, "ease_in_duration");
	dp.packF32(mEaseOut, "ease_out_duration");
	dp.packU32(mHand, "hand_pose");
	dp.packU32(num_joints, "num_joints");

	for (JointVector::iterator ji = mJoints.begin(), end = mJoints.end();
		 ji != end; ++ji)
	{
		Joint* joint = *ji;
		// If ignored, skip it
		if (!joint || joint->mIgnore)
		{
			continue;
		}

		LLQuaternion first_frame_rot;
		LLQuaternion fixup_rot;

		dp.packString(joint->mOutName, "joint_name");
		dp.packS32(joint->mPriority, "joint_priority");

		// Compute coordinate frame rotation
		LLQuaternion frame_rot(joint->mFrameMatrix);
		LLQuaternion frame_rot_inv = ~frame_rot;

		LLQuaternion offset_rot(joint->mOffsetMatrix);

		// Find mergechild and mergeparent joints, if specified
		LLQuaternion merge_parent_rot;
		LLQuaternion merge_child_rot;
		Joint* merge_parent = NULL;
		Joint* merge_child = NULL;

		for (JointVector::iterator mji = mJoints.begin(), mend = mJoints.end();
			 mji != mend; ++mji)
		{
			Joint* mjoint = *mji;
			if (!mjoint) continue;	// Paranoia

			if (!joint->mMergeParentName.empty() &&
				mjoint->mName == joint->mMergeParentName)
			{
				merge_parent = *mji;
			}
			if (!joint->mMergeChildName.empty() &&
				mjoint->mName == joint->mMergeChildName)
			{
				merge_child = *mji;
			}
		}

		dp.packS32(joint->mNumRotKeys, "num_rot_keys");

		LLQuaternion::Order order = bvh_str_to_order(joint->mOrder);
		S32 frame = 0;
		for (KeyVector::iterator ki = joint->mKeys.begin(),
								 kend = joint->mKeys.end();
			 ki != kend; ++ki)
		{
			if (frame == 0 && joint->mRelativeRotationKey)
			{
				first_frame_rot = mayaQ(ki->mRot[0], ki->mRot[1], ki->mRot[2],
										order);

				fixup_rot.shortestArc(LLVector3::z_axis * first_frame_rot *
									  frame_rot, LLVector3::z_axis);
			}

			if (ki->mIgnoreRot)
			{
				++frame;
				continue;
			}

			// Time elapsed before this frame starts.
			F32 time = (F32)(frame - NUMBER_OF_IGNORED_FRAMES_AT_START) *
					   mFrameTime;
			if (time < 0.f)
			{
				time = 0.f;
			}

			if (merge_parent)
			{
				merge_parent_rot = mayaQ(merge_parent->mKeys[frame-1].mRot[0],
									   merge_parent->mKeys[frame-1].mRot[1],
									   merge_parent->mKeys[frame-1].mRot[2],
									   bvh_str_to_order(merge_parent->mOrder));
				LLQuaternion parent_frame_rot(merge_parent->mFrameMatrix);
				LLQuaternion parent_offset_rot(merge_parent->mOffsetMatrix);
				merge_parent_rot = ~parent_frame_rot * merge_parent_rot *
								   parent_frame_rot * parent_offset_rot;
			}
			else
			{
				merge_parent_rot.loadIdentity();
			}

			if (merge_child)
			{
				merge_child_rot = mayaQ(merge_child->mKeys[frame-1].mRot[0],
										merge_child->mKeys[frame-1].mRot[1],
										merge_child->mKeys[frame-1].mRot[2],
										bvh_str_to_order(merge_child->mOrder));
				LLQuaternion child_frame_rot(merge_child->mFrameMatrix);
				LLQuaternion child_offset_rot(merge_child->mOffsetMatrix);
				merge_child_rot = ~child_frame_rot * merge_child_rot *
								  child_frame_rot * child_offset_rot;

			}
			else
			{
				merge_child_rot.loadIdentity();
			}

			LLQuaternion inRot = mayaQ(ki->mRot[0], ki->mRot[1], ki->mRot[2],
									   order);

			LLQuaternion outRot = frame_rot_inv * merge_child_rot * inRot *
								  merge_parent_rot * ~first_frame_rot *
								  frame_rot * offset_rot;

			U16 time_short = F32_to_U16(time, 0.f, mDuration);
			dp.packU16(time_short, "time");
			U16 x, y, z;
			LLVector3 rot_vec = outRot.packToVector3();
			rot_vec.quantize16(-1.f, 1.f, -1.f, 1.f);
			x = F32_to_U16(rot_vec.mV[VX], -1.f, 1.f);
			y = F32_to_U16(rot_vec.mV[VY], -1.f, 1.f);
			z = F32_to_U16(rot_vec.mV[VZ], -1.f, 1.f);
			dp.packU16(x, "rot_angle_x");
			dp.packU16(y, "rot_angle_y");
			dp.packU16(z, "rot_angle_z");
			++frame;
		}

		// Output position keys if joint has motion
		if (!joint->mIgnorePositions)
		{
			dp.packS32(joint->mNumPosKeys, "num_pos_keys");

			LLVector3 rel_pos = joint->mRelativePosition;
			LLVector3 rel_key;

			frame = 0;
			for (KeyVector::iterator ki = joint->mKeys.begin(),
									 kend = joint->mKeys.end();
				 ki != kend; ++ki)
			{
				if (frame == 0 && joint->mRelativePositionKey)
				{
					rel_key.set(ki->mPos);
				}

				if (ki->mIgnorePos)
				{
					++frame;
					continue;
				}

				// Time elapsed before this frame starts.
				F32 time = (F32)(frame - NUMBER_OF_IGNORED_FRAMES_AT_START) *
						   mFrameTime;
				if (time < 0.f)
				{
					time = 0.f;
				}

				LLVector3 in_pos = (LLVector3(ki->mPos) - rel_key) *
								   ~first_frame_rot; // * fixup_rot;
				LLVector3 out_pos = in_pos * frame_rot * offset_rot;

				constexpr F32 INCHES_TO_METERS = 0.02540005f;
				out_pos *= INCHES_TO_METERS;

				out_pos -= rel_pos;
				// SL-318: pelvis position can only move 5m. Limit all joint
				// position offsets to this distance.
				out_pos.clamp(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);

				U16 time_short = F32_to_U16(time, 0.f, mDuration);
				dp.packU16(time_short, "time");

				U16 x, y, z;
				out_pos.quantize16(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET,
								   -LL_MAX_PELVIS_OFFSET,
								   LL_MAX_PELVIS_OFFSET);
				x = F32_to_U16(out_pos.mV[VX], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
				y = F32_to_U16(out_pos.mV[VY], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
				z = F32_to_U16(out_pos.mV[VZ], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
				dp.packU16(x, "pos_x");
				dp.packU16(y, "pos_y");
				dp.packU16(z, "pos_z");

				++frame;
			}
		}
		else
		{
			dp.packS32(0, "num_pos_keys");
		}
	}

	S32 num_constraints = (S32)mConstraints.size();
	dp.packS32(num_constraints, "num_constraints");

	for (ConstraintVector::iterator it = mConstraints.begin(),
									end = mConstraints.end();
		 it != end; ++it)
	{
		U8 byte = it->mChainLength;
		dp.packU8(byte, "chain_length");

		byte = it->mConstraintType;
		dp.packU8(byte, "constraint_type");
		dp.packBinaryDataFixed((U8*)it->mSourceJointName, 16, "source_volume");
		dp.packVector3(it->mSourceOffset, "source_offset");
		dp.packBinaryDataFixed((U8*)it->mTargetJointName, 16, "target_volume");
		dp.packVector3(it->mTargetOffset, "target_offset");
		dp.packVector3(it->mTargetDir, "target_dir");
		dp.packF32(it->mEaseInStart, "ease_in_start");
		dp.packF32(it->mEaseInStop, "ease_in_stop");
		dp.packF32(it->mEaseOutStart, "ease_out_start");
		dp.packF32(it->mEaseOutStop, "ease_out_stop");
	}

	return true;
}
