/**
 * @file llpuppetmodule.cpp
 * @brief Implementation of the LLPuppetModule class.
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

#include "llviewerprecompiledheaders.h"

#include "llpuppetmodule.h"

#include "llanimationstates.h"
#include "llheadrotmotion.h"
#include "llleap.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llpuppetmotion.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

LLPuppetModule::LLPuppetModule()
:	LLEventAPI("puppetry", "Integrate external puppetry control module",
			   "command"),	// Dispatches incoming events on "command" key
	mRange(25.f),
	mPlayServerEcho(false),
	mIsSending(false),
	mIsReceiving(true)
{
	add("get",
		"Puppetry plugin module has requested information from the viewer\n"
		"Requested data may be a simple string.  EX:\n"
		"  camera_id\n"
		"  skeleton\n"
		"Or a key and dict"
		"Response will be a set issued to the plugin module. EX:\n"
		"  camera_id: <integer>\n"
		"  skeleton: <llsd>\n"
		"multiple items may be requested in a single get",
		&LLPuppetModule::processGetRequest);
	add("set",
		"Puppetry plugin module request to apply settings to the viewer.\n"
		"Set data is a structure following the form\n"
		" {'<to_be_set>':<value|structure>}\n"
		"EX: \n"
		"  camera_id: <integer>\n"
		"  joint: {<name>:inverse_kinematics:position[<float>,<float>,<float>]}\n"
		"A set may trigger a set to be issued back to the plugin.\n"
		"multiple pieces of data may be set in a single set.",
		&LLPuppetModule::processSetRequest);

	// This function defines viewer-internal API endpoints for this event
	// handler.
	mSendSkeletonAPI =
		gEventPumps.obtain("SkeletonUpdate")
			.listen("LLPuppetModule",
					[](const LLSD& unused)
					{
						LLPuppetModule::getInstance()->sendSkeleton();
						return false;
					});

	mSendReportAPI =
		gEventPumps.obtain("JointReport")
			.listen("LLPuppetModule",
					[](const LLSD& sd)
					{
						LLPuppetModule::getInstance()->sendReport(sd);
						return false;
					});

	LLControlVariable* controlp =
		gSavedSettings.getControl("PuppetryUseServerEcho");
	if (!controlp)
	{
		llwarns << "Missing \"PuppetryUseServerEcho\" debug variable."
				<< llendl;
		return;
	}
	controlp->getSignal()->connect(boost::bind(&LLPuppetModule::settingsObserver));
}

// Puppetry GET requests are processed here. Expected data format:
//   data = 'command'
//   data = {command:get, data:[thing_one, thing_two, ...]}
//   data = {command:get, d:[thing_one, thing_two, ...]}
//static
void LLPuppetModule::processGetRequest(const LLSD& data)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LL_DEBUGS("PuppetrySpam") << "Puppet data: " << data << llendl;

	// Always check for short format first...
	std::string verb = "d";
	if (!data.has(verb))
	{
		// ... and long format second.
		verb = "data";
		if (!data.has(verb))
		{
			llwarns_sparse << "Missing 'data' key in get request" << llendl;
			return;
		}
	}

	const LLSD& payload = data[verb];
	if (!payload.isArray())
	{
		llwarns_sparse << "Malformed get request: 'data' value is not an array."
					   << llendl;
		return;
	}

	LLPuppetModule* self = LLPuppetModule::getInstance();

	for (LLSD::array_const_iterator it = payload.beginArray(),
									end = payload.endArray();
		 it != end; ++it)
	{
		std::string key = it->asString();
		if (key == "c" || key == "camera")
		{
			// getCameraNumber_ returns results immediately as a Response.
			self->getCameraNumber_(data);
		}
		else if (key == "s" || key == "skeleton")
		{
			self->sendSkeleton(data);
		}
	}
}

// Puppetry SET requests are processed here.
// Expected data format:
//  data = {command:set, data:{inverse_kinematics:{...},joint_state:{...}}
//  data = {command:set, d:{i:{...},j:{...}}
//static
void LLPuppetModule::processSetRequest(const LLSD& data)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LL_DEBUGS("PuppetrySpam") << "Puppet data: " << data << llendl;

	// Always check for short format first...
	std::string verb = "d";
	if (!data.has(verb))
	{
		// ... and long format second.
		verb = "data";
		if (!data.has(verb))
		{
			llwarns_sparse << "Missing 'data' key in set request" << llendl;
			return;
		}
	}

	const LLSD& payload = data[verb];
	if (!payload.isMap())
	{
		llwarns_sparse << "Malformed set request: 'data' value is not a map."
					   << llendl;
		return;
	}

	S32 reqid = data.has("reqid") ? data["reqid"].asInteger() : -1;

	LLPuppetMotion* motionp = gAgentAvatarp->getPuppetMotion();
	if (!motionp)
	{
		llwarns << "No puppet motion found on self" << llendl;
		return;
	}

	LLPuppetModule* self = LLPuppetModule::getInstance();

	for (LLSD::map_const_iterator it = payload.beginMap(),
								  end = payload.endMap();
		 it != end; ++it)
	{
		const std::string& key = it->first;

		if (key == "c" || key == "camera")
		{
			self->setCameraNumber(it->second.asInteger());
			continue;
		}

		const LLSD& joint_data = it->second;
		if (joint_data.isMap())
		{
			self->processJointData(motionp, key, joint_data, reqid);
		}
		else
		{
			llwarns_sparse << "Data is not a map for joint " << key
						   << llendl;
		}
	}
}

void LLPuppetModule::processJointData(LLPuppetMotion* motionp,
									  const std::string& key, const LLSD& data,
									  S32 reqid)
{
	// The reference frame depends on the key
	S32 ref_frame;
	if (key == "i" || key == "inverse_kinematics")
	{
		ref_frame = LLPuppetJointEvent::ROOT_FRAME;
	}
	else if (key == "j" || key == "joint_state")
	{
		ref_frame = LLPuppetJointEvent::PARENT_FRAME;
	}
	else	// Invalid key; ignore...
	{
		llwarns_once << "Invalid key: " << key
					 << ". Expected: i/inverse_kinematics or j/joint_state"
					 <<llendl;
		return;
	}

	for (LLSD::map_const_iterator joint_itr = data.beginMap(),
								  joint_end = data.endMap();
		 joint_itr != joint_end; ++joint_itr)
	{
		std::string joint_name = joint_itr->first;
		if (joint_name.empty())
		{
			continue;
		}

		const LLSD& params = joint_itr->second;
		if (!params.isMap())
		{
			llwarns_once << "Invalid data for joint data key " << joint_name
						 << ". Expected a map but got: " << params << llendl;
			continue;
		}

		LLJoint* joint = NULL;
		if (LLStringOps::isDigit(joint_name[0]))
		{
			// Joint name starts with a digit, try it as a joint_id.
			joint = gAgentAvatarp->getSkeletonJoint(atoi(joint_name.c_str()));
			if (joint)
			{
				joint_name = joint->getName();
			}
		}
		else
		{
			U32 joint_key = LLJoint::getKey(joint_name, false);
			if (joint_key)
			{
				joint = gAgentAvatarp->getJoint(joint_key);
			}
		}
		if (!joint)
		{
			continue;	// Joint not found; ignore...
		}

		if (joint_name == "mHead")
		{
			// If the head is animated, stop looking at the mouse
			disableHeadMotion();
		}

		// Record that we have seen this joint name
		addActiveJoint(joint_name);

		constexpr size_t vx = 0;
		constexpr size_t vy = 1;
		constexpr size_t vz = 2;
		LLPuppetJointEvent joint_event;
		joint_event.setJointID(joint->getJointNum());
		joint_event.setReferenceFrame(ref_frame);
		for (LLSD::map_const_iterator param_itr = params.beginMap(),
									  param_end = params.endMap();
			 param_itr != param_end; ++param_itr)
		{
			const LLSD& value = param_itr->second;
			std::string param_name = param_itr->first;

			constexpr S32 NUM_COMPONENTS = 3;
			if (!value.isArray() || value.size() < NUM_COMPONENTS)
			{
				if (param_name == "d" || param_name == "disable_constraint")
				{
					joint_event.disableConstraint();
				}
				else if (param_name == "r" || param_name == "report")
				{
					// Outputs rot/pos after solution.
					joint_event.enableReporting(reqid);
				}
				continue;
			}

			LLVector3 v(value.get(vx).asReal(), value.get(vy).asReal(),
						value.get(vz).asReal());
			// Sanity-check input value
			constexpr F32 MAX_PUPPETRY_INPUT = 10.f;
			v.clamp(-MAX_PUPPETRY_INPUT, MAX_PUPPETRY_INPUT);

			// Note: LLVector3::clamp() does not protect against NaN input, so
			// we explicitly check it here.
			F32 v_length_squared = v.lengthSquared();
			if (llisnan(v_length_squared))
			{
				continue;
			}

			if (param_name == "r" || param_name == "rotation")
			{
				// Packed quaternions have the imaginary part (xyz)
				LLQuaternion q;
				// Copy the imaginary part
				memcpy(q.mQ, v.mV, 3 * sizeof(F32));
				// Compute the real part
				if (v_length_squared > 1.f)
				{
					F32 inv_length = 1.f / sqrtf(v_length_squared);
					q.mQ[VX] *= inv_length;
					q.mQ[VY] *= inv_length;
					q.mQ[VZ] *= inv_length;
					q.mQ[VW] = 0.f;
				}
				else
				{
					q.mQ[VW] = sqrtf(1.f - v_length_squared);
				}

				joint_event.setRotation(q);
			}
			else if (param_name == "p" || param_name == "position")
			{
				joint_event.setPosition(v);
			}
			else if (param_name == "s" || param_name == "scale")
			{
				joint_event.setScale(v);
			}
		}
		if (!joint_event.isEmpty())
		{
			if (!motionp->isActive())
			{
				gAgentAvatarp->startMotion(ANIM_AGENT_PUPPET_MOTION);
			}
			motionp->addExpressionEvent(joint_event);
		}
	}
}

bool LLPuppetModule::launchLeapPlugin(const std::string& filename)
{
	if (filename.empty() || !LLPuppetMotion::enabled() || havePuppetModule())
	{
		return false;
	}

	// Note: I expanded LLProcess to accept script file names and search for
	// a suitable interpreter to launch (see LLProcess() constructor in the
	// indra/llcommon/lleap.cpp file). It means we do not need to care about it
	// here like LL is doing in their viewer. HB
	std::vector<std::string> command;
	std::string cmd_str = filename;
	command.emplace_back(filename);

	// By default this is "--camera", but I made it configurable via a debug
	// setting; this option can also be omitted in the command line by using
	// an empty string in that setting. HB
	std::string camopt = gSavedSettings.getString("PuppetryCameraOption");
	if (!camopt.empty())
	{
		// Get the camera number.
		std::string camera = llformat("%d", getCameraNumber());

		if (camopt.back() == '=')
		{
			// An option ending with '=' must normally not use spaces to
			// separate it from its parameter and is considered as a single
			// command line option. HB
			camopt += camera;
			command.emplace_back(camopt);
		}
		else
		{
			// The camera option and camera number must be separated with a
			// space and are two distinct command line options.
			command.emplace_back(camopt);
			command.emplace_back(camera);
			camopt += " " + camera;
		}
		cmd_str += " " + camopt;
	}

	llinfos << "Attempting to launch LEAP command: " << cmd_str << llendl;
	try
	{
		LLLeap* leapp = LLLeap::create("Puppetry", command);
		if (leapp)
		{
			leapp->enableBinaryOutput(gSavedSettings.getBool("PuppetryBinaryOutputStream"));
			leapp->enableBinaryInput(gSavedSettings.getBool("PuppetryBinaryInputStream"));
			setLeapModule(leapp->getWeak(), filename);
			llinfos << "Puppetry module successfully created." << llendl;
			setSending(true);
			sendCameraNumber();
			sendSkeleton();
			// Save this valid command, for future potential use... HB
			gSavedSettings.setString("PuppetryLastCommand", cmd_str);
		}
		else	// This should not happen, unless memory could not be allocated
		{
			llwarns << "Failed to launch LEAP module." << llendl;
		}
	}
	catch (const LLLeap::Error& e)
	{
		LLSD args;
		args["COMMAND"] = cmd_str;
		args["ERROR"] = e.what();
		gNotifications.add("LeapModuleFail", args);
		return false;
	}

	return true;
}

bool LLPuppetModule::launchLeapCommand(const std::string& command)
{
	if (command.empty() || !LLPuppetMotion::enabled() || havePuppetModule())
	{
		return false;
	}
	llinfos << "Attempting to launch LEAP command: " << command << llendl;
	try
	{
		LLLeap* leapp = LLLeap::create("Puppetry", command);
		if (leapp)
		{
			leapp->enableBinaryOutput(gSavedSettings.getBool("PuppetryBinaryOutputStream"));
			leapp->enableBinaryInput(gSavedSettings.getBool("PuppetryBinaryInputStream"));
			setLeapModule(leapp->getWeak(), leapp->getExecutable());
			llinfos << "Puppetry module successfully created." << llendl;
			setSending(true);
			sendCameraNumber();
			sendSkeleton();
		}
		else	// This should not happen, unless memory could not be allocated
		{
			llwarns << "Failed to launch LEAP module." << llendl;
		}
	}
	catch (const LLLeap::Error& e)
	{
		LLSD args;
		args["COMMAND"] = command;
		args["ERROR"] = e.what();
		gNotifications.add("LeapModuleFail", args);
		return false;
	}
	return true;
}

void LLPuppetModule::setLeapModule(std::weak_ptr<LLLeap> mod,
								   const std::string& module_name)
{
	mLeapModule = mod;
	mModuleName = module_name;
	mActiveJoints.clear();			// Make sure data is cleared
	if (isAgentAvatarValid())
	{
		LLPuppetMotion* motionp = gAgentAvatarp->getPuppetMotion();
		if (motionp)
		{
			motionp->clearAll();
		}
	}
	// Sync the echo status with the debug setting. HB
	settingsObserver();
}

LLPuppetModule::puppet_module_ptr_t LLPuppetModule::getLeapModule() const
{
	// Lock it
	return mLeapModule.lock();
}

bool LLPuppetModule::havePuppetModule() const
{
	puppet_module_ptr_t mod = getLeapModule();
	return (bool)(mod);
}

void LLPuppetModule::disableHeadMotion() const
{
	if (!isAgentAvatarValid())
	{
		return;
	}
	LLMotion* motionp = gAgentAvatarp->findMotion(ANIM_AGENT_HEAD_ROT);
	if (motionp)
	{
		motionp->disable();
	}
}

void LLPuppetModule::enableHeadMotion() const
{
	if (!isAgentAvatarValid())
	{
		return;
	}
	LLMotion* motionp = gAgentAvatarp->findMotion(ANIM_AGENT_HEAD_ROT);
	if (motionp)
	{
		motionp->enable();
	}
}

void LLPuppetModule::clearLeapModule()
{
	llinfos << "Sending 'stop' command to Leap module" << llendl;
	sendCommand("stop");
	enableHeadMotion();
	mActiveJoints.clear();
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->stopMotion(ANIM_AGENT_PUPPET_MOTION);
	}
	mLeapModule.reset();
}

void LLPuppetModule::sendCommand(const std::string& command,
								 const LLSD& args) const
{
	puppet_module_ptr_t mod = getLeapModule();
	if (mod)
	{
		LLSD data;
		data["command"] = command;
		// args is optional
		if (args.isDefined())
		{
			data["args"] = args;
		}
		LL_DEBUGS("Puppetry") << "Posting to Leap module: " << command
							  << LL_ENDL;
		gEventPumps.post("puppetry.controller", data);
	}
	else
	{
		LL_DEBUGS("Puppetry") << "Puppet module not loaded, dropping command: "
							  << command << LL_ENDL;
	}
}

void LLPuppetModule::setCameraNumber(S32 num)
{
	setCameraNumber_(num);
	// For a C++ caller, also send the new camera number to the LEAP module.
	sendCameraNumber();
}

void LLPuppetModule::setCameraNumber_(S32 num)
{
	gSavedSettings.setS32("PuppetryCamera", num);
	llinfos << "Camera number set to " << num << llendl;
}

S32 LLPuppetModule::getCameraNumber() const
{
	return gSavedSettings.getS32("PuppetryCamera");
}

void LLPuppetModule::getCameraNumber_(const LLSD& request) const
{
	// Response sends a reply on destruction.
	Response response(llsd::map("camera_id", getCameraNumber()), request);
}

void LLPuppetModule::sendCameraNumber()
{
	sendCommand("set_camera", llsd::map("camera_id", getCameraNumber()));
}

void LLPuppetModule::sendReport(const LLSD& sd)
{
	sendCommand("joint_report", sd);
}

void LLPuppetModule::sendSkeleton(const LLSD& sd)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LLPuppetMotion* motionp = gAgentAvatarp->getPuppetMotion();
	if (motionp)
	{
		sendCommand("set_skeleton", motionp->getSkeletonData());
	}
	else
	{
		llwarns << "No puppet motion found on self" << llendl;
	}
}

void LLPuppetModule::sendEnabledParts()
{
	sendCommand("enable_parts", llsd::map("parts_mask", getEnabledPart()));
}

// Enables puppetry on body part: head, face, left/right hands...
void LLPuppetModule::setEnabledPart(S32 part_num, bool enable)
{
	S32 cur_setting = gSavedSettings.getS32("PuppetryParts") & PPM_ALL;
	part_num = part_num & PPM_ALL;
	if (enable)
	{
		cur_setting = cur_setting | part_num;
	}
	else
	{
		cur_setting = cur_setting & ~part_num;
	}

	gSavedSettings.setS32("PuppetryParts", cur_setting);
	llinfos << "Puppetry enabled parts mask now " << cur_setting << llendl;

	sendEnabledParts();	 // Send to module
}

S32 LLPuppetModule::getEnabledPart(S32 mask) const
{
	return gSavedSettings.getS32("PuppetryParts") & mask;
}

void LLPuppetModule::addActiveJoint(const std::string& joint_name)
{
	mActiveJoints[joint_name] = LLFrameTimer::getTotalSeconds();
}

bool LLPuppetModule::isActiveJoint(const std::string& joint_name)
{
	active_joint_map_t::iterator iter = mActiveJoints.find(joint_name);
	if (iter != mActiveJoints.end())
	{
		F64 age = LLFrameTimer::getTotalSeconds() - iter->second;
		const F64 PUPPET_SHOW_BONE_AGE = 3.0;
		if (age < PUPPET_SHOW_BONE_AGE)
		{
			// It was recently active
			return true;
		}
		// Delete old data and return not found
		mActiveJoints.erase(iter);
	}
	return false;   // Not found
}

void LLPuppetModule::setEcho(bool play_server_echo)
{
	setPuppetryOptions(LLSDMap("echo_back", play_server_echo));
}

void LLPuppetModule::setSending(bool sending)
{
	setPuppetryOptions(LLSDMap("transmit", sending));
}

void LLPuppetModule::setReceiving(bool receiving)
{
	setPuppetryOptions(LLSDMap("receive", receiving));
}

void LLPuppetModule::setRange(F32 range)
{
	setPuppetryOptions(LLSDMap("range", range));
}

void LLPuppetModule::setPuppetryOptions(LLSD options)
{
	const std::string& url = gAgent.getRegionCapability("Puppetry");
	if (url.empty())
	{
		llwarns << "No Puppetry capability in this region." << llendl;
		return;
	}

	// Start up coroutine to set puppetry options.
	if (options.has("echo_back") && options["echo_back"].asBoolean())
	{
		// Echo implies both transmit and receive.
		options["transmit"] = true;
		options["receive"] = true;
	}

	gCoros.launch("setPuppetryOptionsCoro",
				  [url, options]()
				  {
						LLPuppetModule::setPuppetryOptionsCoro(url, options);
				  });
}

void LLPuppetModule::parsePuppetryResponse(const LLSD& response)
{
	mPlayServerEcho = response["echo_back"].asBoolean();
	mIsSending = response["transmit"].asBoolean();
	mIsReceiving = response["receive"].asBoolean();
	mRange = response["range"].asReal();

	// *TODO Mute list and subscribe
	llinfos << "Set puppetry parameters from server: echo is "
			<< (mPlayServerEcho ? "on" : "off") << ", transmit is "
			<< (mIsSending ? "on" : "off") << ", receiving is "
			<< (mIsReceiving? "on" : "off") <<  ", receiving range is "
			<< mRange << "m" << llendl;
}

//static
void LLPuppetModule::setPuppetryOptionsCoro(const std::string& url,
											LLSD options)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("setPuppetryOptionsCoro");

	LLSD result;
	LLCore::HttpStatus status;
	S32 retry_count = 0;
	while (true)
	{
		LLSD data = LLSD::emptyMap();
		if (options.has("echo_back"))
		{
			data["echo_back"] = options["echo_back"].asBoolean();
		}
		if (options.has("transmit"))
		{
			data["transmit"] = options["transmit"].asBoolean();
		}
		if (options.has("receive"))
		{
			data["receive"] = options["receive"].asBoolean();
		}
		if (options.has("range"))
		{
			data["range"] = options["range"].asReal();
		}

		result = adapter.postAndSuspend(url, data);

		status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (status.getType() == HTTP_NOT_FOUND)
		{
			// There seems to be a case at first login where the simulator is
			// slow getting all of the caps connected for the agent. It has
			// given us back the cap URL but returns a 404 when we try and hit
			// it. Pause, take a breath and give it another shot.
			if (++retry_count >= 3)
			{
				llwarns << "Failed to set puppetry echo status after 3 retries."
						<< llendl;
				return;
			}
			llcoro::suspendUntilTimeout(0.25f);
		}
		else if (!status)
		{
			llwarns << "Failed to set puppetry echo status with "
					<< status.getMessage() << " - Body: " << result << llendl;
			return;
		}
		else
		{
			break;	// Success
		}
	}

	LLPuppetModule::getInstance()->parsePuppetryResponse(result);
}

// I added a way to remember the echo via a debug setting. Let's observe it and
// sync the echo status when needed. HB
//static
void LLPuppetModule::settingsObserver()
{
	if (!LLPuppetMotion::enabled())
	{
		return;
	}
	LLPuppetModule* self = LLPuppetModule::getInstance();
	if (self->havePuppetModule())
	{
		bool new_echo = gSavedSettings.getBool("PuppetryUseServerEcho");
		self->setEcho(new_echo);
		if (new_echo)
		{
			// If we want echo from the server, we need to have receiving on
			self->setReceiving(true);
		}
	}
}
