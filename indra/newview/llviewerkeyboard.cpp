/**
 * @file llviewerkeyboard.cpp
 * @brief LLViewerKeyboard class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llviewerkeyboard.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llappviewer.h"
#include "llchatbar.h"
#include "llfloatermove.h"
#include "llmorphview.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltoolfocus.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

// Constants
constexpr F32 FLY_TIME = 0.5f;
constexpr F32 FLY_FRAMES = 4.f;
constexpr F32 NUDGE_TIME = 0.25f;		// In seconds
constexpr S32 NUDGE_FRAMES = 2;
constexpr F32 ORBIT_NUDGE_RATE = 0.05f;	// Fraction of normal speed

LLViewerKeyboard gViewerKeyboard;

void agent_jump(EKeystate s)
{
	static bool first_fly_attempt = true;

	if (!gKeyboardp) return;

	if (s == KEYSTATE_UP)
	{
		first_fly_attempt = true;
		return;
	}

	F32 time = gKeyboardp->getCurKeyElapsedTime();
	S32 frame_count = ll_roundp(gKeyboardp->getCurKeyElapsedFrameCount());
	static LLCachedControl<bool> automatic_fly(gSavedSettings, "AutomaticFly");
	if (time < FLY_TIME || frame_count <= FLY_FRAMES || gAgent.upGrabbed() ||
		!automatic_fly)
	{
		gAgent.moveUp(1);
	}
	else
	{
		gAgent.setFlying(true, first_fly_attempt);
		first_fly_attempt = false;
		gAgent.moveUp(1);
	}
}

void agent_push_down(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.moveUp(-1);
	}
}

static void agent_handle_doubletap_run(EKeystate s,
									   LLAgent::EDoubleTapRunMode mode)
{
	if (KEYSTATE_UP == s)
	{
		if (gAgent.mDoubleTapRunMode == mode && gAgent.getRunning() &&
		    !gAgent.getAlwaysRun())
		{
			// Turn off temporary running.
			gAgent.clearRunning();
			gAgent.sendWalkRun(gAgent.getRunning());
		}
	}
	else if (gAllowTapTapHoldRun && KEYSTATE_DOWN == s && !gAgent.getRunning())
	{
		if (gAgent.mDoubleTapRunMode == mode &&
		    gAgent.mDoubleTapRunTimer.getElapsedTimeF32() < NUDGE_TIME)
		{
//MK
			if (!gRLenabled || !gRLInterface.mContainsRun)
			{
//mk
				// Same walk-key was pushed again quickly; this is a
				// double-tap so engage temporary running.
				gAgent.setRunning();
				gAgent.sendWalkRun(gAgent.getRunning());
//MK
			}
//mk
		}

		// Pressing any walk-key resets the double-tap timer
		gAgent.mDoubleTapRunTimer.reset();
		gAgent.mDoubleTapRunMode = mode;
	}
}

static void agent_push_forwardbackward(EKeystate s, S32 direction,
									   LLAgent::EDoubleTapRunMode mode)
{
	agent_handle_doubletap_run(s, mode);
	if (KEYSTATE_UP == s || !gKeyboardp)
	{
		return;
	}

	F32 time = gKeyboardp->getCurKeyElapsedTime();
	S32 frame_count = ll_roundp(gKeyboardp->getCurKeyElapsedFrameCount());

	if (time < NUDGE_TIME || frame_count <= NUDGE_FRAMES)
	{
		gAgent.moveAtNudge(direction);
	}
	else
	{
		gAgent.moveAt(direction);
	}
}

void agent_push_forward(EKeystate s)
{
	agent_push_forwardbackward(s, 1, LLAgent::DOUBLETAP_FORWARD);
}

void agent_push_backward(EKeystate s)
{
	agent_push_forwardbackward(s, -1, LLAgent::DOUBLETAP_BACKWARD);
}

static void agent_slide_leftright(EKeystate s, S32 direction,
								  LLAgent::EDoubleTapRunMode mode)
{
	agent_handle_doubletap_run(s, mode);
	if (KEYSTATE_UP == s || !gKeyboardp) return;

	F32 time = gKeyboardp->getCurKeyElapsedTime();
	S32 frame_count = ll_roundp(gKeyboardp->getCurKeyElapsedFrameCount());

	if (time < NUDGE_TIME || frame_count <= NUDGE_FRAMES)
	{
		gAgent.moveLeftNudge(direction);
	}
	else
	{
		gAgent.moveLeft(direction);
	}
}

void agent_slide_left(EKeystate s)
{
	agent_slide_leftright(s, 1, LLAgent::DOUBLETAP_SLIDELEFT);
}

void agent_slide_right(EKeystate s)
{
	agent_slide_leftright(s, -1, LLAgent::DOUBLETAP_SLIDERIGHT);
}

void agent_turn_left(EKeystate s)
{
	if (gToolFocus.mouseSteerMode())
	{
		agent_slide_left(s);
	}
	else if (s != KEYSTATE_UP && gKeyboardp)
	{
		F32 time = gKeyboardp->getCurKeyElapsedTime();
		gAgent.moveYaw(LLFloaterMove::getYawRate(time));
	}
}

void agent_turn_right(EKeystate s)
{
	if (gToolFocus.mouseSteerMode())
	{
		agent_slide_right(s);
	}
	else if (s != KEYSTATE_UP && gKeyboardp)
	{
		F32 time = gKeyboardp->getCurKeyElapsedTime();
		gAgent.moveYaw(-LLFloaterMove::getYawRate(time));
	}
}

void agent_look_up(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.movePitch(-1);
	}
}

void agent_look_down(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.movePitch(1);
	}
}

void agent_toggle_fly(EKeystate s)
{
	// Only catch the edge
	if (s == KEYSTATE_DOWN)
	{
		gAgent.toggleFlying();
	}
}

F32 get_orbit_rate()
{
	if (gKeyboardp)
	{
		F32 time = gKeyboardp->getCurKeyElapsedTime();
		if (time < NUDGE_TIME)
		{
			return ORBIT_NUDGE_RATE +
				   time * (1 - ORBIT_NUDGE_RATE) / NUDGE_TIME;
		}
	}

	return 1.f;
}

void camera_spin_around_ccw(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitLeftKey(get_orbit_rate());
	}
}

void camera_spin_around_cw(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitRightKey(get_orbit_rate());
	}
}

void camera_spin_around_ccw_sitting(EKeystate s)
{
	if (KEYSTATE_UP == s &&
		gAgent.mDoubleTapRunMode != LLAgent::DOUBLETAP_SLIDERIGHT)
	{
		return;
	}
	if (gAgent.rotateGrabbed() || gAgent.sitCameraEnabled() ||
		gAgent.getRunning())
	{
		// Send keystrokes, but do not change camera
		agent_turn_right(s);
	}
	else
	{
		// Change camera but do not send keystrokes
		gAgent.setOrbitLeftKey(get_orbit_rate());
	}
}

void camera_spin_around_cw_sitting(EKeystate s)
{
	if (KEYSTATE_UP == s &&
		gAgent.mDoubleTapRunMode != LLAgent::DOUBLETAP_SLIDELEFT)
	{
		return;
	}
	if (gAgent.rotateGrabbed() || gAgent.sitCameraEnabled() ||
		gAgent.getRunning())
	{
		// Send keystrokes, but do not change camera
		agent_turn_left(s);
	}
	else
	{
		// Change camera but do not send keystrokes
		gAgent.setOrbitRightKey(get_orbit_rate());
	}
}

void camera_spin_over(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitUpKey(get_orbit_rate());
	}
}

void camera_spin_under(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitDownKey(get_orbit_rate());
	}
}

void camera_spin_over_sitting(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		if (gAgent.upGrabbed() || gAgent.sitCameraEnabled())
		{
			// Send keystrokes, but do not change camera
			agent_jump(s);
		}
		else
		{
			// Change camera but do not send keystrokes
			gAgent.setOrbitUpKey(get_orbit_rate());
		}
	}
}

void camera_spin_under_sitting(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		if (gAgent.downGrabbed() || gAgent.sitCameraEnabled())
		{
			// Send keystrokes, but do not change camera
			agent_push_down(s);
		}
		else
		{
			// Change camera but do not send keystrokes
			gAgent.setOrbitDownKey(get_orbit_rate());
		}
	}
}

void camera_move_forward(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitInKey(get_orbit_rate());
	}
}

void camera_move_backward(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitOutKey(get_orbit_rate());
	}
}

void camera_move_forward_sitting(EKeystate s)
{
	if (KEYSTATE_UP == s &&
		gAgent.mDoubleTapRunMode != LLAgent::DOUBLETAP_FORWARD)
	{
		return;
	}
	if (gAgent.forwardGrabbed() || gAgent.sitCameraEnabled() ||
		(gAgent.getRunning() && !gAgent.getAlwaysRun()))
	{
		agent_push_forward(s);
	}
	else
	{
		gAgent.setOrbitInKey(get_orbit_rate());
	}
}

void camera_move_backward_sitting(EKeystate s)
{
	if (KEYSTATE_UP == s &&
		gAgent.mDoubleTapRunMode != LLAgent::DOUBLETAP_BACKWARD)
	{
		return;
	}
	if (gAgent.backwardGrabbed() || gAgent.sitCameraEnabled() ||
		(gAgent.getRunning() && !gAgent.getAlwaysRun()))
	{
		agent_push_backward(s);
	}
	else
	{
		gAgent.setOrbitOutKey(get_orbit_rate());
	}
}

void camera_pan_up(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanUpKey(get_orbit_rate());
	}
}

void camera_pan_down(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanDownKey(get_orbit_rate());
	}
}

void camera_pan_left(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanLeftKey(get_orbit_rate());
	}
}

void camera_pan_right(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanRightKey(get_orbit_rate());
	}
}

void camera_pan_in(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanInKey(get_orbit_rate());
	}
}

void camera_pan_out(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setPanOutKey(get_orbit_rate());
	}
}

void camera_move_forward_fast(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitInKey(2.5f);
	}
}

void camera_move_backward_fast(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gAgent.unlockView();
		gAgent.setOrbitOutKey(2.5f);
	}
}

void edit_avatar_spin_ccw(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitLeftKey(get_orbit_rate());
	}
}

void edit_avatar_spin_cw(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitRightKey(get_orbit_rate());
	}
}

void edit_avatar_spin_over(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitUpKey(get_orbit_rate());
	}
}

void edit_avatar_spin_under(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitDownKey(get_orbit_rate());
	}
}

void edit_avatar_move_forward(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitInKey(get_orbit_rate());
	}
}

void edit_avatar_move_backward(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		gMorphViewp->setCameraDrivenByKeys(true);
		gAgent.setOrbitOutKey(get_orbit_rate());
	}
}

void stop_moving(EKeystate s)
{
	if (s != KEYSTATE_UP)
	{
		// Stop agent
		gAgent.setControlFlags(AGENT_CONTROL_STOP);
		// Cancel autopilot
		gAgentPilot.stopAutoPilot();
	}
}

void start_chat(EKeystate s)
{
	LLChatBar::startChat(NULL);
}

void start_gesture(EKeystate s)
{
	LLUICtrl* focused = gFocusMgr.getKeyboardFocusUICtrl();
	if (gChatBarp && KEYSTATE_UP == s &&
		!(focused && focused->acceptsTextInput()))
	{
		if (gChatBarp->getCurrentChat().empty())
		{
			// No existing chat in chat editor, insert '/'
			LLChatBar::startChat("/");
		}
		else
		{
			// Do not overwrite existing text in chat editor
			LLChatBar::startChat(NULL);
		}
	}
}

void bind_keyboard_functions()
{
	gViewerKeyboard.bindNamedFunction("jump", agent_jump);
	gViewerKeyboard.bindNamedFunction("push_down", agent_push_down);
	gViewerKeyboard.bindNamedFunction("push_forward", agent_push_forward);
	gViewerKeyboard.bindNamedFunction("push_backward", agent_push_backward);
	gViewerKeyboard.bindNamedFunction("look_up", agent_look_up);
	gViewerKeyboard.bindNamedFunction("look_down", agent_look_down);
	gViewerKeyboard.bindNamedFunction("toggle_fly", agent_toggle_fly);
	gViewerKeyboard.bindNamedFunction("turn_left", agent_turn_left);
	gViewerKeyboard.bindNamedFunction("turn_right", agent_turn_right);
	gViewerKeyboard.bindNamedFunction("slide_left", agent_slide_left);
	gViewerKeyboard.bindNamedFunction("slide_right", agent_slide_right);
	gViewerKeyboard.bindNamedFunction("spin_around_ccw",
									  camera_spin_around_ccw);
	gViewerKeyboard.bindNamedFunction("spin_around_cw", camera_spin_around_cw);
	gViewerKeyboard.bindNamedFunction("spin_around_ccw_sitting",
									  camera_spin_around_ccw_sitting);
	gViewerKeyboard.bindNamedFunction("spin_around_cw_sitting",
									  camera_spin_around_cw_sitting);
	gViewerKeyboard.bindNamedFunction("spin_over", camera_spin_over);
	gViewerKeyboard.bindNamedFunction("spin_under", camera_spin_under);
	gViewerKeyboard.bindNamedFunction("spin_over_sitting",
									  camera_spin_over_sitting);
	gViewerKeyboard.bindNamedFunction("spin_under_sitting",
									  camera_spin_under_sitting);
	gViewerKeyboard.bindNamedFunction("move_forward", camera_move_forward);
	gViewerKeyboard.bindNamedFunction("move_backward", camera_move_backward);
	gViewerKeyboard.bindNamedFunction("move_forward_sitting",
									  camera_move_forward_sitting);
	gViewerKeyboard.bindNamedFunction("move_backward_sitting",
									  camera_move_backward_sitting);
	gViewerKeyboard.bindNamedFunction("pan_up", camera_pan_up);
	gViewerKeyboard.bindNamedFunction("pan_down", camera_pan_down);
	gViewerKeyboard.bindNamedFunction("pan_left", camera_pan_left);
	gViewerKeyboard.bindNamedFunction("pan_right", camera_pan_right);
	gViewerKeyboard.bindNamedFunction("pan_in", camera_pan_in);
	gViewerKeyboard.bindNamedFunction("pan_out", camera_pan_out);
	gViewerKeyboard.bindNamedFunction("move_forward_fast",
									  camera_move_forward_fast);
	gViewerKeyboard.bindNamedFunction("move_backward_fast",
									  camera_move_backward_fast);
	gViewerKeyboard.bindNamedFunction("edit_avatar_spin_ccw",
									  edit_avatar_spin_ccw);
	gViewerKeyboard.bindNamedFunction("edit_avatar_spin_cw",
									  edit_avatar_spin_cw);
	gViewerKeyboard.bindNamedFunction("edit_avatar_spin_over",
									  edit_avatar_spin_over);
	gViewerKeyboard.bindNamedFunction("edit_avatar_spin_under",
									  edit_avatar_spin_under);
	gViewerKeyboard.bindNamedFunction("edit_avatar_move_forward",
									  edit_avatar_move_forward);
	gViewerKeyboard.bindNamedFunction("edit_avatar_move_backward",
									  edit_avatar_move_backward);
	gViewerKeyboard.bindNamedFunction("stop_moving", stop_moving);
	gViewerKeyboard.bindNamedFunction("start_chat", start_chat);
	gViewerKeyboard.bindNamedFunction("start_gesture", start_gesture);
}

LLViewerKeyboard::LLViewerKeyboard()
:	mNamedFunctionCount(0)
{
	for (S32 i = 0; i < MODE_COUNT; ++i)
	{
		mBindingCount[i] = 0;
	}

	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		mKeyHandledByUI[i] = false;
	}
	// We want the UI to never see these keys so that they can always control
	// the avatar/camera
	for (KEY k = KEY_PAD_UP; k <= KEY_PAD_DIVIDE; ++k)
	{
		mKeysSkippedByUI.insert(k);
	}
}

void LLViewerKeyboard::bindNamedFunction(const std::string& name,
										 LLKeyFunc func)
{
	S32 i = mNamedFunctionCount++;
	mNamedFunctions[i].mName = name;
	mNamedFunctions[i].mFunction = func;
}

bool LLViewerKeyboard::modeFromString(const std::string& string, S32* mode)
{
	if (string == "FIRST_PERSON")
	{
		*mode = MODE_FIRST_PERSON;
		return true;
	}
	else if (string == "THIRD_PERSON")
	{
		*mode = MODE_THIRD_PERSON;
		return true;
	}
	else if (string == "EDIT")
	{
		*mode = MODE_EDIT;
		return true;
	}
	else if (string == "EDIT_AVATAR")
	{
		*mode = MODE_EDIT_AVATAR;
		return true;
	}
	else if (string == "SITTING")
	{
		*mode = MODE_SITTING;
		return true;
	}
	else
	{
		*mode = MODE_THIRD_PERSON;
		return false;
	}
}

bool LLViewerKeyboard::handleKey(KEY translated_key, MASK translated_mask,
								 bool repeated)
{
	// check for re-map
	EKeyboardMode mode = gViewerKeyboard.getMode();
	U32 keyidx = (translated_mask << 16) | translated_key;
	key_remap_t::iterator iter = mRemapKeys[mode].find(keyidx);
	if (iter != mRemapKeys[mode].end())
	{
		translated_key = iter->second & 0xff;
		translated_mask = iter->second >> 16;
	}

	// No repeats of F-keys
	bool repeatable_key = translated_key < KEY_F1 || translated_key > KEY_F12;
	if (!repeatable_key && repeated)
	{
		return false;
	}

	LL_DEBUGS("UserInput") << "keydown: " << translated_key << LL_ENDL;
	// Skip skipped keys
	if (mKeysSkippedByUI.find(translated_key) != mKeysSkippedByUI.end())
	{
		mKeyHandledByUI[translated_key] = false;
	}
	else if (gViewerWindowp)
	{
		// It is sufficient to set this value once per call to handlekey
		// without clearing it, as it is only used in the subsequent call to
		// scanKey
		mKeyHandledByUI[translated_key] =
			gViewerWindowp->handleKey(translated_key, translated_mask);
	}
	else
	{
		return false;
	}

	return mKeyHandledByUI[translated_key];
}

bool LLViewerKeyboard::handleKeyUp(KEY translated_key, MASK translated_mask)
{
	return gViewerWindowp &&
		   gViewerWindowp->handleKeyUp(translated_key, translated_mask);
}

bool LLViewerKeyboard::bindKey(S32 mode, KEY key, MASK mask,
							   const std::string& function_name)
{
	S32 index;
	void (*function)(EKeystate keystate) = NULL;
	std::string name;

	// Allow remapping of F2-F12
	if (function_name[0] == 'F')
	{
		S32 c1 = function_name[1] - '0';
		S32 c2 = function_name[2] ? function_name[2] - '0' : -1;
		if (c1 >= 0 && c1 <= 9 && c2 >= -1 && c2 <= 9)
		{
			S32 idx = c1;
			if (c2 >= 0)
			{
				idx = idx * 10 + c2;
			}
			if (idx >= 2 && idx <= 12)
			{
				U32 keyidx = (mask << 16) | key;
				(mRemapKeys[mode])[keyidx] = KEY_F1 + idx - 1;
				return true;
			}
		}
	}

	// Not remapped, look for a function
	for (S32 i = 0; i < mNamedFunctionCount; ++i)
	{
		if (function_name == mNamedFunctions[i].mName)
		{
			function = mNamedFunctions[i].mFunction;
			name = mNamedFunctions[i].mName;
		}
	}

	if (!function)
	{
		llerrs << "Cannot bind key to function " << function_name
			   << ", no function with this name found" << llendl;
	}

	// Check for duplicate first and overwrite
	for (index = 0; index < mBindingCount[mode]; ++index)
	{
		if (key == mBindings[mode][index].mKey &&
			mask == mBindings[mode][index].mMask)
		{
			break;
		}
	}

	if (index >= MAX_KEY_BINDINGS)
	{
		llerrs << "Too many keys for mode " << mode << llendl;
	}

	if (mode >= MODE_COUNT)
	{
		llerrs << "Unknown mode: " << mode << llendl;
	}

	mBindings[mode][index].mKey = key;
	mBindings[mode][index].mMask = mask;
#if 0
 	mBindings[mode][index].mName = name;
#endif
	mBindings[mode][index].mFunction = function;

	if (index == mBindingCount[mode])
	{
		++mBindingCount[mode];
	}

	return true;
}

S32 LLViewerKeyboard::loadBindings(const std::string& filename)
{
	if (filename.empty())
	{
		llerrs << "No filename specified" << llendl;
		return 0;
	}

	LLFILE* fp = LLFile::open(filename, "r");
	if (!fp)
	{
		return 0;
	}

	constexpr S32 BUFFER_SIZE = 2048;
	char buffer[BUFFER_SIZE];
	// *NOTE: This buffer size is hard coded into scanf() below.
	char mode_string[MAX_STRING] = "";
	char key_string[MAX_STRING] = "";
	char mask_string[MAX_STRING] = "";
	char function_string[MAX_STRING] = "";
	S32 mode = MODE_THIRD_PERSON;
	KEY key = 0;
	MASK mask = 0;
	S32 binding_count = 0;
	S32 line_count = 0;
	while (!feof(fp))
	{
		++line_count;
		if (!fgets(buffer, BUFFER_SIZE, fp))
		{
			break;
		}

		// Skip over comments, blank lines
		if (buffer[0] == '#' || buffer[0] == '\n') continue;

		// Grab the binding strings
		S32 tokens_read = sscanf(buffer, "%254s %254s %254s %254s",
								 mode_string, key_string, mask_string,
								 function_string);

		if (tokens_read == EOF)
		{
			llwarns << "Unexpected end-of-file at line " << line_count
					<< " of key binding file " << filename << llendl;
			LLFile::close(fp);
			return 0;
		}
		else if (tokens_read < 4)
		{
			llwarns << "Cannot read line " << line_count
					<< " of key binding file " << filename << llendl;
			continue;
		}

		// Convert mode
		if (!modeFromString(mode_string, &mode))
		{
			llwarns << "Unknown mode on line " << line_count
					<< " of key binding file " << filename << llendl;
			llinfos << "Mode must be one of FIRST_PERSON, THIRD_PERSON, EDIT, EDIT_AVATAR"
					<< llendl;
			continue;
		}

		// Convert key
		if (!LLKeyboard::keyFromString(key_string, &key))
		{
			llwarns << "Cannot interpret key on line " << line_count
					<< " of key binding file " << filename << llendl;
			continue;
		}

		// Convert mask
		if (!LLKeyboard::maskFromString(mask_string, &mask))
		{
			llwarns << "Cannot interpret mask on line " << line_count
					<< " of key binding file " << filename << llendl;
			continue;
		}

		// bind key
		if (bindKey(mode, key, mask, function_string))
		{
			++binding_count;
		}
	}

	LLFile::close(fp);

	return binding_count;
}

EKeyboardMode LLViewerKeyboard::getMode()
{
	if (gAgent.cameraMouselook())
	{
		return MODE_FIRST_PERSON;
	}
	if (gMorphViewp && gMorphViewp->getVisible())
	{
		return MODE_EDIT_AVATAR;
	}
	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{
		return MODE_SITTING;
	}
	return MODE_THIRD_PERSON;
}

// Called from scanKeyboard.
void LLViewerKeyboard::scanKey(KEY key, bool key_down, bool key_up,
							   bool key_level)
{
	if (!gKeyboardp) return;

	S32 mode = getMode();
	// Consider keyboard scanning as NOT mouse event. JC
	MASK mask = gKeyboardp->currentMask(false);

	LLKeyBinding* binding = mBindings[mode];
	S32 binding_count = mBindingCount[mode];

	if (mKeyHandledByUI[key])
	{
		return;
	}

	// Do not process key down on repeated keys
	bool repeat = gKeyboardp->getKeyRepeated(key);

	for (S32 i = 0; i < binding_count; ++i)
	{
		if (binding[i].mKey == key)
		{
			if (binding[i].mMask == mask)
			{
				if (key_down && !repeat)
				{
					// Key went down this frame, call function
					(*binding[i].mFunction)(KEYSTATE_DOWN);
				}
				else if (key_up)
				{
					// Key went down this frame, call function
					(*binding[i].mFunction)(KEYSTATE_UP);
				}
				else if (key_level)
				{
					// Key held down from previous frame
					// Not windows, just call the function.
					(*binding[i].mFunction)(KEYSTATE_LEVEL);
				}
			}
		}
	}
}
