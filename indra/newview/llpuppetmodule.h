/**
 * @file llpuppetmodule.h
 * @brief Declaration of the LLPuppetModule class.
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

#ifndef LL_LLPUPPETMODULE_H
#define LL_LLPUPPETMODULE_H

#include <memory>
#include <map>

#include "lleventdispatcher.h"			// For LLEventAPI
#include "llsingleton.h"

class LLLeap;
class LLPuppetMotion;

// Bit mask for puppetry parts enabling
enum LLPuppetPartMask
{
	// These must match the (void*)user_data parameter used for the corresponding
	// LLMenuItemCheckGL() calls in llviewermenu.cpp, init_puppetry_menu()
	// function.
	PPM_NONE		=  0,
	PPM_HEAD		=  1,
	PPM_FACE		=  2,
	PPM_LEFT_HAND	=  4,
	PPM_RIGHT_HAND	=  8,
	PPM_FINGERS		= 16,
	PPM_ALL			= 31
};

// Singleton to manage a pointer to the LLLeap module that provides puppetry
// functions
class LLPuppetModule final : public LLSingleton<LLPuppetModule>,
							 public LLEventAPI
{
	friend class LLSingleton<LLPuppetModule>;

protected:
	LOG_CLASS(LLPuppetModule);

public:
	LLPuppetModule();
	~LLPuppetModule() override = default;

	// Used to launch a LEAP plugin/script as a puppet module. Whenever the
	// "PuppetryCameraOption" debug setting is not empty, the string it
	// contains is automatically added as an option, together with the current
	// camera number. This method returns true when successful or false
	// otherwise, and spawns an alert dialog in case of failure. HB
	bool launchLeapPlugin(const std::string& filename);
	// Used to launch a LEAP module with the provided command line (i.e. an
	// executable or script file name and any needed options) as a puppet
	// module. Returns true when successful or false otherwise, and spawns an
	// alert dialog in case of failure. HB
	bool launchLeapCommand(const std::string& command);

	typedef std::shared_ptr<LLLeap> puppet_module_ptr_t;
	void setLeapModule(std::weak_ptr<LLLeap> mod,
					   const std::string& module_name);
	puppet_module_ptr_t getLeapModule() const;

	bool havePuppetModule() const;		// Returns true when module is loaded
	void disableHeadMotion() const;
	void enableHeadMotion() const;
	void clearLeapModule();

	void sendCommand(const std::string& command,
					 const LLSD& args = LLSD()) const;

	LL_INLINE const std::string& getModuleName() const	{ return mModuleName; }

	 // Enable puppetry on body part - head, face, left/right hands
	void setEnabledPart(S32 part_num, bool enable);
	S32 getEnabledPart(S32 mask = PPM_ALL) const;

	void setCameraNumber(S32 num);
	S32 getCameraNumber() const;

	void sendCameraNumber();
	void sendEnabledParts();
	void sendSkeleton(const LLSD& sd = LLSD::emptyMap());
	void sendReport(const LLSD& sd = LLSD::emptyMap());

	LL_INLINE bool getEcho() const						{ return mPlayServerEcho; }
	void setEcho(bool play_server_echo);

	LL_INLINE bool isSending() const					{ return mIsSending; }
	void setSending(bool sending);

	LL_INLINE bool isReceiving() const					{ return mIsReceiving; }
	void setReceiving(bool receiving);

	LL_INLINE F32 getRange() const						{ return mRange; }
	void setRange(F32 range);

	 // Map of used joints and last time
	typedef std::map<std::string, F64> active_joint_map_t;
	void addActiveJoint(const std::string& joint_name);
	bool isActiveJoint(const std::string& joint_name);

	LL_INLINE const active_joint_map_t& getActiveJoints() const
	{
		return mActiveJoints;
	}

	void parsePuppetryResponse(const LLSD& response);

private:
	void processJointData(LLPuppetMotion* motionp, const std::string& key,
						  const LLSD& data, S32 reqid = -1);

	void setCameraNumber_(S32 num);						// LEAP caller
	void getCameraNumber_(const LLSD& request) const;	// LEAP caller

	void setPuppetryOptions(LLSD options);
	static void setPuppetryOptionsCoro(const std::string& url, LLSD options);

	static void processGetRequest(const LLSD& data);
	static void processSetRequest(const LLSD& data);
	static void settingsObserver();

private:
	mutable std::weak_ptr<LLLeap>	mLeapModule;	// Weak pointer to module

	// For event pump to send leap updates to plug-ins
	LLTempBoundListener				mSendSkeletonAPI;
	LLTempBoundListener				mSendReportAPI;

	std::string						mModuleName;

	// Sent to the expression module on request.
	LLSD							mSkeletonData;

	// Map of used joints and last time seen
	active_joint_map_t				mActiveJoints;

	F32								mRange;

	// true to play own avatar from server data stream, not directy from leap
	// module.
	bool							mPlayServerEcho;
	// true when streaming to simulator
	bool							mIsSending;
	// true when getting stream from simulator
	bool							mIsReceiving;
};

#endif
