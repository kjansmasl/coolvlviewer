/**
 * @file llcontrol.cpp
 * @brief Holds global state for viewer.
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

#include "linden_common.h"

#include <iostream>
#include <fstream>
#include <algorithm>

#include "llcontrol.h"

#include "llstl.h"

#include "llstring.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llcolor4u.h"
#include "llcolor4.h"
#include "llcolor3.h"
#include "llrect.h"
#include "llxmltree.h"
#include "llsdserialize.h"

#if LL_DEBUG
#define CONTROL_ERRS llerrs
#else
#define CONTROL_ERRS llwarns
#endif

template<> eControlType get_control_type<U32>();
template<> eControlType get_control_type<S32>();
template<> eControlType get_control_type<F32>();
template<> eControlType get_control_type<bool>();
template<> eControlType get_control_type<std::string>();

template<> eControlType get_control_type<LLVector3>();
template<> eControlType get_control_type<LLVector3d>();
template<> eControlType get_control_type<LLRect>();
template<> eControlType get_control_type<LLColor4>();
template<> eControlType get_control_type<LLColor3>();
template<> eControlType get_control_type<LLColor4U>();
template<> eControlType get_control_type<LLSD>();

template<> LLSD convert_to_llsd<U32>(const U32& in);
template<> LLSD convert_to_llsd<LLVector3>(const LLVector3& in);
template<> LLSD convert_to_llsd<LLVector3d>(const LLVector3d& in);
template<> LLSD convert_to_llsd<LLRect>(const LLRect& in);
template<> LLSD convert_to_llsd<LLColor4>(const LLColor4& in);
template<> LLSD convert_to_llsd<LLColor3>(const LLColor3& in);
template<> LLSD convert_to_llsd<LLColor4U>(const LLColor4U& in);

template<> bool convert_from_llsd<bool>(const LLSD& sd, eControlType type,
										const char* control_name);

template<> S32 convert_from_llsd<S32>(const LLSD& sd, eControlType type,
									  const char* control_name);

template<> U32 convert_from_llsd<U32>(const LLSD& sd, eControlType type,
									  const char* control_name);

template<> F32 convert_from_llsd<F32>(const LLSD& sd, eControlType type,
									  const char* control_name);

template<> std::string convert_from_llsd<std::string>(const LLSD& sd,
													  eControlType type,
													  const char* control_name);

template<> LLWString convert_from_llsd<LLWString>(const LLSD& sd,
												  eControlType type,
												  const char* control_name);

template<> LLVector3 convert_from_llsd<LLVector3>(const LLSD& sd,
												  eControlType type,
												  const char* control_name);

template<> LLVector3d convert_from_llsd<LLVector3d>(const LLSD& sd,
													eControlType type,
													const char* control_name);

template<> LLRect convert_from_llsd<LLRect>(const LLSD& sd,
											eControlType type,
											const char* control_name);

template<> LLColor4 convert_from_llsd<LLColor4>(const LLSD& sd,
												eControlType type,
												const char* control_name);

template<> LLColor4U convert_from_llsd<LLColor4U>(const LLSD& sd,
												  eControlType type,
												  const char* control_name);

template<> LLColor3 convert_from_llsd<LLColor3>(const LLSD& sd,
												eControlType type,
												const char* control_name);

template<> LLSD convert_from_llsd<LLSD>(const LLSD& sd, eControlType type,
										const char* control_name);

// This defines the current version of the settings file
constexpr S32 CURRENT_VERSION = 101;

bool LLControlVariable::llsd_compare(const LLSD& a, const LLSD& b)
{
	bool result = false;
	switch (mType)
	{
		case TYPE_U32:
		case TYPE_S32:
			result = a.asInteger() == b.asInteger();
			break;

		case TYPE_BOOLEAN:
			result = a.asBoolean() == b.asBoolean();
			break;

		case TYPE_F32:
			result = a.asReal() == b.asReal();
			break;

		case TYPE_VEC3:
		case TYPE_VEC3D:
			result = LLVector3d(a) == LLVector3d(b);
			break;

		case TYPE_RECT:
			result = LLRect(a) == LLRect(b);
			break;

		case TYPE_COL4:
			result = LLColor4(a) == LLColor4(b);
			break;

		case TYPE_COL3:
			result = LLColor3(a) == LLColor3(b);
			break;

		case TYPE_COL4U:
			result = LLColor4U(a) == LLColor4U(b);
			break;

		case TYPE_STRING:
			result = a.asString() == b.asString();
			break;

		default:
			break;
	}

	return result;
}

LLControlVariable::LLControlVariable(const char* name, eControlType type,
									 LLSD initial, const std::string& comment,
									 bool persist, bool hide_from_user)
:	mName(name),
	mComment(comment),
	mType(type),
	mPersist(persist),
	mHideFromUser(hide_from_user)
{
	if (mPersist && mComment.empty())
	{
		llerrs << "Must supply a comment for control " << mName << llendl;
	}
	// Push back versus setValue'ing here, since we don't want to call a signal
	// yet
	mValues.push_back(initial);
}

LLSD LLControlVariable::getComparableValue(const LLSD& value)
{
	// *FIXME: the following is needed to make the LLSD::ImplString work with
	// boolean controls...
	LLSD storable_value;
	if (type() == TYPE_BOOLEAN && value.isString())
	{
		bool temp;
		if (LLStringUtil::convertToBool(value.asString(), temp))
		{
			storable_value = temp;
		}
		else
		{
			storable_value = false;
		}
	}
	else if (type() == TYPE_LLSD && value.isString())
	{
		LLPointer<LLSDNotationParser> parser = new LLSDNotationParser;
		LLSD result;
		std::stringstream value_stream(value.asString());
		if (parser->parse(value_stream, result,
						  LLSDSerialize::SIZE_UNLIMITED) != LLSDParser::PARSE_FAILURE)
		{
			storable_value = result;
		}
		else
		{
			storable_value = value;
		}
	}
	else
	{
		storable_value = value;
	}

	return storable_value;
}

void LLControlVariable::setValue(const LLSD& new_value, bool saved_value)
{
	if (!mValidateSignal(this, new_value))
	{
		// Cannot set new value, exit
		return;
	}

	LLSD storable_value = getComparableValue(new_value);
	bool value_changed = !llsd_compare(getValue(), storable_value);
	if (saved_value)
	{
		// If we are going to save this value, return to default but do not
		// fire
		resetToDefault(false);
		if (!llsd_compare(mValues.back(), storable_value))
		{
			mValues.push_back(storable_value);
		}
	}
	else
	{
		// This is a unsaved value. It needs to reside at mValues[2] (or
		// greater). It must not affect the result of getSaveValue()
		if (!llsd_compare(mValues.back(), storable_value))
		{
			while (mValues.size() > 2)
			{
				// Remove any unsaved values.
				mValues.pop_back();
			}

			if (mValues.size() < 2)
			{
				// Add the default to the 'save' value.
				mValues.push_back(mValues[0]);
			}

			// Add the 'un-save' value.
			mValues.push_back(storable_value);
		}
	}

	if (value_changed)
	{
		mCommitSignal(this, storable_value);
	}
}

void LLControlVariable::setDefaultValue(const LLSD& value)
{
	// Set the control variables value and make it
	// the default value. If the active value is changed,
	// send the signal.
	// *NOTE: Default values are not saved, only read.

	LLSD comparable_value = getComparableValue(value);
	bool value_changed = !llsd_compare(getValue(), comparable_value);
	resetToDefault(false);
	mValues[0] = comparable_value;
	if (value_changed)
	{
		firePropertyChanged();
	}
}

void LLControlVariable::setPersist(bool state)
{
	mPersist = state;
}

void LLControlVariable::setHiddenFromUser(bool hide)
{
	mHideFromUser = hide;
}

void LLControlVariable::setComment(const std::string& comment)
{
	mComment = comment;
}

void LLControlVariable::resetToDefault(bool fire_signal)
{
	//The first setting is always the default
	//Pop to it and fire off the listener
	while (mValues.size() > 1)
	{
		mValues.pop_back();
	}

	if (fire_signal)
	{
		firePropertyChanged();
	}
}

bool LLControlVariable::isSaveValueDefault()
{
    return mValues.size() == 1 ||
		   (mValues.size() > 1 && llsd_compare(mValues[1], mValues[0]));
}

LLSD LLControlVariable::getSaveValue(bool user_value) const
{
	// The first level of the stack is default. We assume that the second level
	// is user preferences that should be saved
	return user_value && mValues.size() > 1 ? mValues[1] : mValues[0];
}

LLControlVariablePtr LLControlGroup::getControl(const char* name)
{
	if (!name || !*name)
	{
		return LLControlVariablePtr();
	}
	ctrl_name_table_t::iterator iter = mNameTable.find(name);
	return iter == mNameTable.end() ? LLControlVariablePtr() : iter->second;
}

////////////////////////////////////////////////////////////////////////////

LLControlGroup::LLControlGroup(const std::string& name)
:	LLInstanceTracker<LLControlGroup, std::string>(name)
{
	mTypeString[TYPE_U32] = "U32";
	mTypeString[TYPE_S32] = "S32";
	mTypeString[TYPE_F32] = "F32";
	mTypeString[TYPE_BOOLEAN] = "Boolean";
	mTypeString[TYPE_STRING] = "String";
	mTypeString[TYPE_VEC3] = "Vector3";
    mTypeString[TYPE_VEC3D] = "Vector3D";
	mTypeString[TYPE_RECT] = "Rect";
	mTypeString[TYPE_COL4] = "Color4";
	mTypeString[TYPE_COL3] = "Color3";
	mTypeString[TYPE_COL4U] = "Color4u";
	mTypeString[TYPE_LLSD] = "LLSD";
}

LLControlGroup::~LLControlGroup()
{
	cleanup();
}

void LLControlGroup::cleanup()
{
	mNameTable.clear();
}

eControlType LLControlGroup::typeStringToEnum(const std::string& typestr)
{
	for (U32 i = 0; i < (U32)TYPE_COUNT; ++i)
	{
		if (mTypeString[i] == typestr)
		{
			return (eControlType)i;
		}
	}
	return (eControlType)-1;
}

std::string LLControlGroup::typeEnumToString(eControlType typeenum)
{
	return mTypeString[typeenum];
}

LLControlVariable* LLControlGroup::declareControl(const char* name,
												  eControlType type,
												  const LLSD initial_val,
												  const std::string& comment,
												  bool persist,
												  bool hide_from_user)
{
	LLControlVariable* controlp = getControl(name);
	if (!controlp)
 	{
		// If is does not yet exist, create the control and add it to the name
		// table
		controlp = new LLControlVariable(name, type, initial_val, comment,
										 persist, hide_from_user);
		mNameTable.emplace(name, controlp);
	}
	// Sometimes we need to declare a control *after* it has been loaded from a
	// settings file.
	else if (persist && controlp->isType(type))
	{
		if (!controlp->llsd_compare(controlp->getDefault(), initial_val))
		{
			// Get the current value:
			LLSD cur_value = controlp->getValue();
			// Set the default to the declared value:
			controlp->setDefaultValue(initial_val);
			// Now set to the loaded value
			controlp->setValue(cur_value);
		}
	}
	else
	{
		llwarns << "Control named " << name
				<< " already exists; ignoring new declaration." << llendl;
	}
	return controlp;
}

LLControlVariable* LLControlGroup::declareBool(const char* name,
											   bool initial_val,
											   const std::string& comment,
											   bool persist)
{
	return declareControl(name, TYPE_BOOLEAN, initial_val, comment, persist);
}

LLControlVariable* LLControlGroup::declareString(const char* name,
												 const std::string& initial_val,
												 const std::string& comment,
												 bool persist)
{
	return declareControl(name, TYPE_STRING, initial_val, comment, persist);
}

LLControlVariable* LLControlGroup::declareColor4U(const char* name,
												  const LLColor4U& initial_val,
												  const std::string& comment,
												  bool persist)
{
	return declareControl(name, TYPE_COL4U, initial_val.getValue(), comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareColor4(const char* name,
												 const LLColor4& initial_val,
												 const std::string& comment,
												 bool persist)
{
	return declareControl(name, TYPE_COL4, initial_val.getValue(), comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareLLSD(const char* name,
											   const LLSD& initial_val,
											   const std::string& comment,
											   bool persist)
{
	return declareControl(name, TYPE_LLSD, initial_val, comment, persist);
}

#if 0	// Not used
LLControlVariable* LLControlGroup::declareU32(const char* name,
											  U32 initial_val,
											  const std::string& comment,
											  bool persist)
{
	return declareControl(name, TYPE_U32, (LLSD::Integer) initial_val, comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareS32(const char* name,
											  S32 initial_val,
											  const std::string& comment,
											  bool persist)
{
	return declareControl(name, TYPE_S32, initial_val, comment, persist);
}

LLControlVariable* LLControlGroup::declareF32(const char* name,
											  F32 initial_val,
											  const std::string& comment,
											  bool persist)
{
	return declareControl(name, TYPE_F32, initial_val, comment, persist);
}

LLControlVariable* LLControlGroup::declareVec3(const char* name,
											   const LLVector3& initial_val,
											   const std::string& comment,
											   bool persist)
{
	return declareControl(name, TYPE_VEC3, initial_val.getValue(), comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareVec3d(const char* name,
												const LLVector3d& initial_val,
												const std::string& comment,
												bool persist)
{
	return declareControl(name, TYPE_VEC3D, initial_val.getValue(), comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareRect(const char* name,
											   const LLRect& initial_val,
											   const std::string& comment,
											   bool persist)
{
	return declareControl(name, TYPE_RECT, initial_val.getValue(), comment,
						  persist);
}

LLControlVariable* LLControlGroup::declareColor3(const char* name,
												 const LLColor3& initial_val,
												 const std::string& comment,
												 bool persist)
{
	return declareControl(name, TYPE_COL3, initial_val.getValue(), comment,
						  persist);
}
#endif

bool LLControlGroup::getBool(const char* name)
{
	return get<bool>(name);
}

S32 LLControlGroup::getS32(const char* name)
{
	return get<S32>(name);
}

U32 LLControlGroup::getU32(const char* name)
{
	return get<U32>(name);
}

F32 LLControlGroup::getF32(const char* name)
{
	return get<F32>(name);
}

std::string LLControlGroup::getString(const char* name)
{
	return get<std::string>(name);
}

LLWString LLControlGroup::getWString(const char* name)
{
	return get<LLWString>(name);
}

std::string LLControlGroup::getText(const char* name)
{
	std::string utf8_string = getString(name);
	LLStringUtil::replaceChar(utf8_string, '^', '\n');
	LLStringUtil::replaceChar(utf8_string, '%', ' ');
	return (utf8_string);
}

LLVector3 LLControlGroup::getVector3(const char* name)
{
	return get<LLVector3>(name);
}

LLVector3d LLControlGroup::getVector3d(const char* name)
{
	return get<LLVector3d>(name);
}

LLRect LLControlGroup::getRect(const char* name)
{
	return get<LLRect>(name);
}

LLColor4 LLControlGroup::getColor(const char* name)
{
	LL_DEBUGS("GetControlCalls") << "Requested control: " << name << LL_ENDL;

	ctrl_name_table_t::const_iterator i = mNameTable.find(name);

	if (i != mNameTable.end())
	{
		LLControlVariable* controlp = i->second;

		switch (controlp->mType)
		{
			case TYPE_COL4:
			{
				return LLColor4(controlp->getValue());
			}
			case TYPE_COL4U:
			{
				return LLColor4(LLColor4U(controlp->getValue()));
			}
			default:
			{
				CONTROL_ERRS << "Control " << name << " not a color" << llendl;
				return LLColor4::white;
			}
		}
	}
	else
	{
		CONTROL_ERRS << "Invalid getColor control " << name << llendl;
		return LLColor4::white;
	}
}

LLColor4 LLControlGroup::getColor4(const char* name)
{
	return get<LLColor4>(name);
}

LLColor4U LLControlGroup::getColor4U(const char* name)
{
	return get<LLColor4U>(name);
}

LLColor3 LLControlGroup::getColor3(const char* name)
{
	return get<LLColor3>(name);
}

LLSD LLControlGroup::getLLSD(const char* name)
{
	return get<LLSD>(name);
}

bool LLControlGroup::controlExists(const char* name)
{
	ctrl_name_table_t::iterator iter = mNameTable.find(name);
	return iter != mNameTable.end();
}

//-------------------------------------------------------------------
// Set functions
//-------------------------------------------------------------------

void LLControlGroup::setBool(const char* name, bool val)
{
	set(name, val);
}

void LLControlGroup::setS32(const char* name, S32 val)
{
	set(name, val);
}

void LLControlGroup::setF32(const char* name, F32 val)
{
	set(name, val);
}

void LLControlGroup::setU32(const char* name, U32 val)
{
	set(name, val);
}

void LLControlGroup::setString(const char* name, const std::string& val)
{
	set(name, val);
}

void LLControlGroup::setVector3(const char* name, const LLVector3& val)
{
	set(name, val);
}

void LLControlGroup::setVector3d(const char* name, const LLVector3d& val)
{
	set(name, val);
}

void LLControlGroup::setRect(const char* name, const LLRect& val)
{
	set(name, val);
}

void LLControlGroup::setColor4(const char* name, const LLColor4& val)
{
	set(name, val);
}

void LLControlGroup::setLLSD(const char* name, const LLSD& val)
{
	set(name, val);
}

void LLControlGroup::setUntypedValue(const char* name, const LLSD& val)
{
	if (!name || !*name)
	{
		return;
	}

	LLControlVariable* controlp = getControl(name);
	if (controlp)
	{
		controlp->setValue(val);
	}
	else
	{
		CONTROL_ERRS << "Invalid control " << name << llendl;
	}
}

//---------------------------------------------------------------
// Load and save
//---------------------------------------------------------------

// Returns number of controls loaded, so 0 if failure
U32 LLControlGroup::loadFromFileLegacy(const std::string& filename,
									   bool require_declaration,
									   eControlType declare_as)
{
	std::string name;

	LLXmlTree xml_controls;

	if (!xml_controls.parseFile(filename))
	{
		llwarns << "Unable to open control file: " << filename << llendl;
		return 0;
	}

	LLXmlTreeNode* rootp = xml_controls.getRoot();
	if (!rootp || !rootp->hasAttribute("version"))
	{
		llwarns << "No valid settings header found in control file: "
				 << filename << llendl;
		return 0;
	}

	U32 validitems = 0;
	S32 version;

	rootp->getAttributeS32("version", version);

	// Check file version
	if (version != CURRENT_VERSION)
	{
		llinfos << filename << " does not appear to be a version "
				<< CURRENT_VERSION << " controls file" << llendl;
		return 0;
	}

	LLXmlTreeNode* child_nodep = rootp->getFirstChild();
	while (child_nodep)
	{
		name = child_nodep->getName();

		bool declared = controlExists(name.c_str());
		if (require_declaration && !declared)
		{
			// Declaration required, but this name not declared.
			// Complain about non-empty names.
			if (!name.empty())
			{
				// Read in to end of line
				llwarns << "Trying to set \"" << name
						<< "\", setting doesn't exist." << llendl;
			}
			child_nodep = rootp->getNextChild();
			continue;
		}

		// Got an item. Load it up.

		// If not declared, assume it is a string
		if (!declared)
		{
			switch (declare_as)
			{
				case TYPE_COL4:
					declareColor4(name.c_str(), LLColor4::white,
								  LLStringUtil::null, NO_PERSIST);
					break;

				case TYPE_COL4U:
					declareColor4U(name.c_str(), LLColor4U::white,
								   LLStringUtil::null, NO_PERSIST);
					break;

				case TYPE_STRING:
				default:
					declareString(name.c_str(), LLStringUtil::null,
								  LLStringUtil::null, NO_PERSIST);
			}
		}

		// Control name has been declared in code.
		LLControlVariable* controlp = getControl(name.c_str());
		llassert(controlp);

		switch (controlp->mType)
		{
			case TYPE_F32:
			{
				F32 initial = 0.f;
				child_nodep->getAttributeF32("value", initial);
				controlp->setValue(initial);
				controlp->setDefaultValue(initial);
				++validitems;
				break;
			}

			case TYPE_S32:
			{
				S32 initial = 0;
				child_nodep->getAttributeS32("value", initial);
				controlp->setValue(initial);
				controlp->setDefaultValue(initial);
				++validitems;
				break;
			}

			case TYPE_U32:
			{
				U32 initial = 0;
				child_nodep->getAttributeU32("value", initial);
				controlp->setValue((LLSD::Integer) initial);
				controlp->setDefaultValue((LLSD::Integer)initial);
				++validitems;
				break;
			}

			case TYPE_BOOLEAN:
			{
				bool initial = false;
				child_nodep->getAttributeBool("value", initial);
				controlp->setValue(initial);
				controlp->setDefaultValue(initial);
				++validitems;
				break;
			}

			case TYPE_STRING:
			{
				std::string string;
				child_nodep->getAttributeString("value", string);
				controlp->setValue(string);
				controlp->setDefaultValue(string);
				++validitems;
				break;
			}

			case TYPE_VEC3:
			{
				LLVector3 vector;
				child_nodep->getAttributeVector3("value", vector);
				controlp->setValue(vector.getValue());
				controlp->setDefaultValue(vector.getValue());
				++validitems;
				break;
			}

			case TYPE_VEC3D:
			{
				LLVector3d vector;
				child_nodep->getAttributeVector3d("value", vector);
				controlp->setValue(vector.getValue());
				controlp->setDefaultValue(vector.getValue());
				++validitems;
				break;
			}

			case TYPE_RECT:
			{
				// RN: hack to support reading rectangles from a string
				std::string rect_string;
				child_nodep->getAttributeString("value", rect_string);
				std::istringstream istream(rect_string);
				S32 left, bottom, width, height;
				istream >> left >> bottom >> width >> height;
				LLRect rect;
				rect.setOriginAndSize(left, bottom, width, height);
				controlp->setValue(rect.getValue());
				controlp->setDefaultValue(rect.getValue());
				++validitems;
				break;
			}

			case TYPE_COL4U:
			{
				LLColor4U color;
				child_nodep->getAttributeColor4U("value", color);
				controlp->setValue(color.getValue());
				controlp->setDefaultValue(color.getValue());
				++validitems;
				break;
			}

			case TYPE_COL4:
			{
				LLColor4 color;
				child_nodep->getAttributeColor4("value", color);
				controlp->setValue(color.getValue());
				controlp->setDefaultValue(color.getValue());
				++validitems;
				break;
			}

			case TYPE_COL3:
			{
				LLVector3 color;
				child_nodep->getAttributeVector3("value", color);
				controlp->setValue(LLColor3(color.mV).getValue());
				controlp->setDefaultValue(LLColor3(color.mV).getValue());
				++validitems;
				break;
			}

			default:
				break;
		}

		child_nodep = rootp->getNextChild();
	}

	return validitems;
}

U32 LLControlGroup::saveToFile(const std::string& filename,
							   bool nondefault_only,
							   bool save_default)
{
	LLSD settings;
	U32 num_saved = 0;
	for (ctrl_name_table_t::iterator iter = mNameTable.begin(),
									 end = mNameTable.end();
		 iter != end; ++iter)
	{
		LLControlVariable* controlp = iter->second;
		if (!controlp)
		{
			llwarns << "Tried to save invalid control: " << iter->first
					<< llendl;
		}

		if (controlp && (save_default || controlp->isPersisted()))
		{
			if (!nondefault_only || !controlp->isSaveValueDefault())
			{
				settings[iter->first]["Comment"] = controlp->getComment();
				if (save_default)
				{
					settings[iter->first]["Persist"] =
						LLSD(controlp->isPersisted());
					if (controlp->isHiddenFromUser())
					{
						settings[iter->first]["HideFromEditor"] = LLSD(true);
					}
				}
				settings[iter->first]["Type"] =
					typeEnumToString(controlp->type());
				LLSD value = controlp->getSaveValue(!save_default);
				// Let's make sure we save the value as its actual type (which
				// might not be the case for booelans, integers and floats...):
				LLSD true_value;
				switch (controlp->type())
				{
					case TYPE_BOOLEAN:
						true_value = LLSD(value.asBoolean());
						break;

					case TYPE_U32:
					case TYPE_S32:
						true_value = LLSD(value.asInteger());
						break;

					case TYPE_F32:
						true_value = LLSD(value.asReal());
						break;

					default:
						true_value = value;
				}
				settings[iter->first]["Value"] = true_value;
				++num_saved;
			}
			else
			{
				LL_DEBUGS("SaveSettings") << "Skipping " << controlp->getName()
										  << LL_ENDL;
			}
		}
	}
	llofstream file(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::toPrettyXML(settings, file);
		file.close();
		llinfos << "Saved to " << filename << llendl;
	}
	else
	{
        // This is a warning because sometime we want to use settings files
		// which cannot be written...
		llwarns << "Unable to open settings file: " << filename << llendl;
		return 0;
	}
	return num_saved;
}

U32 LLControlGroup::loadFromFile(const std::string& filename,
								 bool set_default_values, bool save_values)
{
	llifstream infile(filename.c_str());
	if (!infile.is_open())
	{
		llwarns << "Cannot find file " << filename << " to load." << llendl;
		return 0;
	}

	LLSD settings;
	if (LLSDSerialize::fromXML(settings, infile) == LLSDParser::PARSE_FAILURE)
	{
		infile.close();
		llwarns << "Unable to parse LLSD control file " << filename
				<< ". Trying the legacy method." << llendl;
		return loadFromFileLegacy(filename, true, TYPE_STRING);
	}

	U32	validitems = 0;
	bool hide_from_editor = false;

	for (LLSD::map_const_iterator itr = settings.beginMap(),
								  end = settings.endMap();
		 itr != end; ++itr)
	{
		bool persist = true;
		const std::string& name = itr->first;
		const LLSD& control_map = itr->second;

		if (control_map.has("Persist"))
		{
			persist = control_map["Persist"].asInteger();
		}

		// Sometimes we want to use the settings system to provide cheap
		// persistence, but we do not want the settings themselves to be easily
		// manipulated in the UI because doing so can cause support problems.
		// So we have this option:
		if (control_map.has("HideFromEditor"))
		{
			hide_from_editor = control_map["HideFromEditor"].asInteger();
		}
		else
		{
			hide_from_editor = false;
		}

		// If the control exists just set the value from the input file.
		LLControlVariable* controlp = getControl(name.c_str());
		if (controlp)
		{
			if (set_default_values)
			{
				// Override all previously set properties of this control.
				// ... except for type. The types must match.
				eControlType new_type =
					typeStringToEnum(control_map["Type"].asString());
				if (controlp->isType(new_type))
				{
					controlp->setDefaultValue(control_map["Value"]);
					controlp->setPersist(persist);
					controlp->setHiddenFromUser(hide_from_editor);
					controlp->setComment(control_map["Comment"].asString());
				}
				else
				{
					llerrs << "Mismatched type of control variable '" << name
						   << "' found while loading '" << filename << "'."
						   << llendl;
				}
			}
			else if (controlp->isPersisted())
			{
				controlp->setValue(control_map["Value"], save_values);
			}
			// *NOTE: If not persisted and not setting defaults,
			// the value should not get loaded.
		}
		else
		{
			declareControl(name.c_str(),
						   typeStringToEnum(control_map["Type"].asString()),
						   control_map["Value"],
						   control_map["Comment"].asString(),
						   persist, hide_from_editor);
		}

		++validitems;
	}

	return validitems;
}

void LLControlGroup::resetToDefaults()
{
	for (ctrl_name_table_t::iterator it = mNameTable.begin(),
									 end = mNameTable.end();
		 it != end; ++it)
	{
		(it->second)->resetToDefault();
	}
}

void LLControlGroup::applyToAll(ApplyFunctor* func)
{
	for (ctrl_name_table_t::iterator iter = mNameTable.begin();
		 iter != mNameTable.end(); iter++)
	{
		func->apply(iter->first, iter->second);
	}
}

template<> eControlType get_control_type<U32>()
{
	return TYPE_U32;
}

template<> eControlType get_control_type<S32>()
{
	return TYPE_S32;
}

template<> eControlType get_control_type<F32>()
{
	return TYPE_F32;
}

template<> eControlType get_control_type<bool>()
{
	return TYPE_BOOLEAN;
}

template<> eControlType get_control_type<std::string>()
{
	return TYPE_STRING;
}

template<> eControlType get_control_type<LLVector3>()
{
	return TYPE_VEC3;
}

template<> eControlType get_control_type<LLVector3d>()
{
	return TYPE_VEC3D;
}

template<> eControlType get_control_type<LLRect>()
{
	return TYPE_RECT;
}

template<> eControlType get_control_type<LLColor4>()
{
	return TYPE_COL4;
}

template<> eControlType get_control_type<LLColor4U>()
{
	return TYPE_COL4U;
}

template<> eControlType get_control_type<LLColor3>()
{
	return TYPE_COL3;
}

template<> eControlType get_control_type<LLSD>()
{
	return TYPE_LLSD;
}

template<> LLSD convert_to_llsd<U32>(const U32& in)
{
	return (LLSD::Integer)in;
}

template<> LLSD convert_to_llsd<LLVector3>(const LLVector3& in)
{
	return in.getValue();
}

template<> LLSD convert_to_llsd<LLVector3d>(const LLVector3d& in)
{
	return in.getValue();
}

template<> LLSD convert_to_llsd<LLRect>(const LLRect& in)
{
	return in.getValue();
}

template<> LLSD convert_to_llsd<LLColor4>(const LLColor4& in)
{
	return in.getValue();
}

template<> LLSD convert_to_llsd<LLColor4U>(const LLColor4U& in)
{
	return in.getValue();
}

template<> LLSD convert_to_llsd<LLColor3>(const LLColor3& in)
{
	return in.getValue();
}

template<> bool convert_from_llsd<bool>(const LLSD& sd, eControlType type,
										const char* control_name)
{
	if (type == TYPE_BOOLEAN)
	{
		return sd.asBoolean();
	}
	CONTROL_ERRS << "Invalid bool value for " << control_name << ": " << sd
				 << llendl;
	if (type == TYPE_S32 || type == TYPE_U32)
	{
		return sd.asInteger() != 0;
	}
	return false;
}

template<> S32 convert_from_llsd<S32>(const LLSD& sd, eControlType type,
						 			  const char* control_name)
{
	// *HACK: TYPE_U32 needed for LLCachedControl<U32> !
	if (type == TYPE_S32 || type == TYPE_U32)
	{
		return sd.asInteger();
	}
	CONTROL_ERRS << "Invalid S32 value for " << control_name << ": " << sd
				 << llendl;
	return 0;
}

template<> U32 convert_from_llsd<U32>(const LLSD& sd, eControlType type,
									  const char* control_name)
{
	if (type == TYPE_U32)
	{
		return sd.asInteger();
	}
	CONTROL_ERRS << "Invalid U32 value for " << control_name << ": " << sd
				 << llendl;
	if (type == TYPE_S32 && sd.asInteger() >= 0)
	{
		return sd.asInteger();
	}
	return 0;
}

template<> F32 convert_from_llsd<F32>(const LLSD& sd, eControlType type,
									  const char* control_name)
{
	if (type == TYPE_F32)
	{
		return (F32)sd.asReal();
	}
	CONTROL_ERRS << "Invalid F32 value for " << control_name << ": " << sd
				 << llendl;
	if (type == TYPE_S32 || type == TYPE_U32)
	{
		return (F32)sd.asInteger();
	}
	return 0.0f;
}

template<> std::string convert_from_llsd<std::string>(const LLSD& sd,
													  eControlType type,
													  const char* control_name)
{
	if (type == TYPE_STRING)
	{
		return sd.asString();
	}
	CONTROL_ERRS << "Invalid string value for " << control_name << ": "
				 << sd << llendl;
	return LLStringUtil::null;
}

template<> LLWString convert_from_llsd<LLWString>(const LLSD& sd,
												  eControlType type,
												  const char* control_name)
{
	return utf8str_to_wstring(convert_from_llsd<std::string>(sd, type, control_name));
}

template<> LLVector3 convert_from_llsd<LLVector3>(const LLSD& sd,
												  eControlType type,
												  const char* control_name)
{
	if (type == TYPE_VEC3)
	{
		return (LLVector3)sd;
	}
	CONTROL_ERRS << "Invalid LLVector3 value for " << control_name << ": "
				 << sd << llendl;
	return LLVector3::zero;
}

template<> LLVector3d convert_from_llsd<LLVector3d>(const LLSD& sd,
													eControlType type,
													const char* control_name)
{
	if (type == TYPE_VEC3D)
	{
		return (LLVector3d)sd;
	}
	CONTROL_ERRS << "Invalid LLVector3d value for " << control_name << ": "
				 << sd << llendl;
	return LLVector3d::zero;
}

template<> LLRect convert_from_llsd<LLRect>(const LLSD& sd, eControlType type,
											const char* control_name)
{
	if (type == TYPE_RECT)
	{
		return LLRect(sd);
	}
	CONTROL_ERRS << "Invalid rect value for " << control_name << ": " << sd
				 << llendl;
	return LLRect::null;
}

template<> LLColor4 convert_from_llsd<LLColor4>(const LLSD& sd,
												eControlType type,
												const char* control_name)
{
	if (type == TYPE_COL4)
	{
		LLColor4 color(sd);
		if (color.mV[VRED] < 0.f || color.mV[VRED] > 1.f)
		{
			llwarns << "Color " << control_name
					<< " red value out of range: " << color << llendl;
		}
		if (color.mV[VGREEN] < 0.f || color.mV[VGREEN] > 1.f)
		{
			llwarns << "Color " << control_name
					<< " green value out of range: " << color << llendl;
		}
		if (color.mV[VBLUE] < 0.f || color.mV[VBLUE] > 1.f)
		{
			llwarns << "Color " << control_name
					<< " blue value out of range: " << color << llendl;
		}
		if (color.mV[VALPHA] < 0.f || color.mV[VALPHA] > 1.f)
		{
			llwarns << "Color " << control_name
					<< " alpha value out of range: " << color << llendl;
		}
		return LLColor4(sd);
	}
	CONTROL_ERRS << "Control " << control_name << " not a color" << llendl;
	return LLColor4::white;
}

template<> LLColor4U convert_from_llsd<LLColor4U>(const LLSD& sd,
												  eControlType type,
												  const char* control_name)
{
	if (type == TYPE_COL4U)
	{
		return LLColor4U(sd);
	}
	CONTROL_ERRS << "Invalid LLColor4U value for " << control_name << ": "
				 << sd << llendl;
	return LLColor4U::white;
}

template<> LLColor3 convert_from_llsd<LLColor3>(const LLSD& sd,
												eControlType type,
												const char* control_name)
{
	if (type == TYPE_COL3)
	{
		return sd;
	}
	CONTROL_ERRS << "Invalid LLColor3 value for " << control_name << ": "
				 << sd << llendl;
	return LLColor3::white;
}

template<> LLSD convert_from_llsd<LLSD>(const LLSD& sd, eControlType type,
										const char* control_name)
{
	return sd;
}

//============================================================================
// First-use

static std::string get_warn_name(const std::string& name)
{
	std::string warnname = "Warn" + name;
	for (std::string::iterator iter = warnname.begin(); iter != warnname.end();
		 ++iter)
	{
		char c = *iter;
		if (!isalnum(c))
		{
			*iter = '_';
		}
	}
	return warnname;
}

// Note: may get called more than once per warning (e.g. if already loaded from
// a settings file), but that is OK, declareBool will handle it.
void LLControlGroup::addWarning(const std::string& name)
{
	std::string warnname = get_warn_name(name);
	std::string comment = "Enables " + name + " warning dialog";
	declareBool(warnname.c_str(), true, comment);
	mWarnings.emplace(warnname);
}

bool LLControlGroup::getWarning(const std::string& name)
{
	std::string warnname = get_warn_name(name);
	return getBool(warnname.c_str());
}

void LLControlGroup::setWarning(const std::string& name, bool val)
{
	std::string warnname = get_warn_name(name);
	setBool(warnname.c_str(), val);
}

void LLControlGroup::resetWarnings()
{
	for (std::set<std::string>::iterator iter = mWarnings.begin();
		 iter != mWarnings.end(); ++iter)
	{
		setBool(iter->c_str(), true);
	}
}
