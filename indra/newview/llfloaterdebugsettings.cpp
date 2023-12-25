/**
 * @file llfloaterdebugsettings.cpp
 * @brief floater for debugging internal viewer settings
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

#include "llfloaterdebugsettings.h"

#include "llcombobox.h"
#include "lllineeditor.h"
#include "llspinctrl.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llcolorswatch.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"

LLFloaterDebugSettings::LLFloaterDebugSettings(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_settings_debug.xml");
}

bool LLFloaterDebugSettings::postBuild()
{
	mComboNames = getChild<LLComboBox>("settings_combo");

	struct f final : public LLControlGroup::ApplyFunctor
	{
		LLComboBox* combo;
		f(LLComboBox* c) : combo(c) {}
		void apply(const std::string& name, LLControlVariable* ctrl) override
		{
			if (!ctrl->isHiddenFromUser())
			{
				combo->add(name, (void*)ctrl);
			}
		}
	} func(mComboNames);

	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	gColors.applyToAll(&func);

	mComboNames->sortByName();
	mComboNames->setCommitCallback(onSettingSelect);
	mComboNames->setCallbackUserData(this);
	mComboNames->selectFirstItem();

	childSetCommitCallback("val_spinner_1", onCommitSettings);
	childSetUserData("val_spinner_1", this);
	childSetCommitCallback("val_spinner_2", onCommitSettings);
	childSetUserData("val_spinner_2", this);
	childSetCommitCallback("val_spinner_3", onCommitSettings);
	childSetUserData("val_spinner_3", this);
	childSetCommitCallback("val_spinner_4", onCommitSettings);
	childSetUserData("val_spinner_4", this);
	childSetCommitCallback("val_text", onCommitSettings);
	childSetUserData("val_text", this);
	childSetCommitCallback("boolean_combo", onCommitSettings);
	childSetUserData("boolean_combo", this);
	childSetCommitCallback("color_swatch", onCommitSettings);
	childSetUserData("color_swatch", this);
	childSetAction("default_btn", onClickDefault, this);
	mComment = getChild<LLTextEditor>("comment_text");

	LLSearchEditor* search = getChild<LLSearchEditor>("control_search");
	search->setSearchCallback(onSearchEdit, this);

	return true;
}

void LLFloaterDebugSettings::draw()
{
	updateControl((LLControlVariable*)mComboNames->getCurrentUserdata());
	LLFloater::draw();
}

//static
void LLFloaterDebugSettings::onSettingSelect(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterDebugSettings* self = (LLFloaterDebugSettings*)user_data;
	LLComboBox* combo_box = (LLComboBox*)ctrl;
	if (self && ctrl)
	{
		LLControlVariable* controlp;
		controlp = (LLControlVariable*)combo_box->getCurrentUserdata();
		self->updateControl(controlp);
	}
}

//static
void LLFloaterDebugSettings::onSearchEdit(const std::string& search_string,
										  void* user_data)
{
	static std::string filter;

	LLFloaterDebugSettings* self = (LLFloaterDebugSettings*)user_data;
	if (!self) return;

	filter = search_string;
	LLStringUtil::trim(filter);
	LLStringUtil::toLower(filter);

	struct f final : public LLControlGroup::ApplyFunctor
	{
		LLComboBox* combo;
		f(LLComboBox* c)
		:	combo(c)
		{
		}

		void apply(const std::string& name, LLControlVariable* ctrl) override
		{
			if (!ctrl->isHiddenFromUser())
			{
				std::string setting_name = name;
				LLStringUtil::toLower(setting_name);
				if (filter.empty() ||
					setting_name.find(filter) != std::string::npos)
				{
					combo->add(name, (void*)ctrl);
				}
			}
		}
	} func(self->mComboNames);

	self->mComboNames->removeall();

	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	gColors.applyToAll(&func);

	self->mComboNames->sortByName();
	self->mComboNames->selectFirstItem();
}

//MK
// If the debug setting associated with controlp can be changed through RLV and
// a setdebug restriction is active, return false. Else return true.
bool canChangeSettingRLV(LLControlVariable* controlp)
{
	if (!controlp || !gRLenabled || !gRLInterface.mContainsSetdebug)
	{
		return true;
	}

	std::string name = controlp->getName();
	LLStringUtil::toLower(name);
	std::string tmp;
	for (S32 i = 0, count = gRLInterface.mAllowedSetDebug.size();
		 i < count; ++i)
	{
		tmp = gRLInterface.mAllowedSetDebug[i];
		LLStringUtil::toLower(tmp);
		if (tmp == name)
		{
			return false;
		}
	}

	return true;
}
//mk

//static
void LLFloaterDebugSettings::onCommitSettings(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterDebugSettings* self = (LLFloaterDebugSettings*)user_data;
	if (!self) return;

	LLControlVariable* controlp =
		(LLControlVariable*)self->mComboNames->getCurrentUserdata();
	if (!controlp) return;

//MK
	// If this debug setting can be changed through RLV and a setdebug
	// restriction is active, ignore the change
	if (!canChangeSettingRLV(controlp))
	{
		return;
	}
//mk

	LLVector3 vector;
	LLVector3d vectord;
	LLRect rect;
	LLColor4 col4;
	LLColor3 col3;
	LLColor4U col4U;
	LLColor4 color_with_alpha;

	switch (controlp->type())
	{
		case TYPE_U32:
			controlp->setValue(self->childGetValue("val_spinner_1"));
			break;

		case TYPE_S32:
			controlp->setValue(self->childGetValue("val_spinner_1"));
			break;

		case TYPE_F32:
			controlp->setValue(LLSD(self->childGetValue("val_spinner_1").asReal()));
			break;

		case TYPE_BOOLEAN:
			controlp->setValue(self->childGetValue("boolean_combo"));
			break;

		case TYPE_STRING:
			controlp->setValue(LLSD(self->childGetValue("val_text").asString()));
			break;

		case TYPE_VEC3:
			vector.mV[VX] = (F32)self->childGetValue("val_spinner_1").asReal();
			vector.mV[VY] = (F32)self->childGetValue("val_spinner_2").asReal();
			vector.mV[VZ] = (F32)self->childGetValue("val_spinner_3").asReal();
			controlp->setValue(vector.getValue());
			break;

		case TYPE_VEC3D:
			vectord.mdV[VX] = self->childGetValue("val_spinner_1").asReal();
			vectord.mdV[VY] = self->childGetValue("val_spinner_2").asReal();
			vectord.mdV[VZ] = self->childGetValue("val_spinner_3").asReal();
			controlp->setValue(vectord.getValue());
			break;

		case TYPE_RECT:
			rect.mLeft = self->childGetValue("val_spinner_1").asInteger();
			rect.mRight = self->childGetValue("val_spinner_2").asInteger();
			rect.mBottom = self->childGetValue("val_spinner_3").asInteger();
			rect.mTop = self->childGetValue("val_spinner_4").asInteger();
			controlp->setValue(rect.getValue());
			break;

		case TYPE_COL4:
			col3.setValue(self->childGetValue("color_swatch"));
			col4 = LLColor4(col3, (F32)self->childGetValue("val_spinner_4").asReal());
			controlp->setValue(col4.getValue());
			break;

		case TYPE_COL3:
			controlp->setValue(self->childGetValue("color_swatch"));
#if 0
			col3.mV[VRED] = (F32)self->childGetValue("val_spinner_1").asC();
			col3.mV[VGREEN] = (F32)self->childGetValue("val_spinner_2").asReal();
			col3.mV[VBLUE] = (F32)self->childGetValue("val_spinner_3").asReal();
			controlp->setValue(col3.getValue());
#endif
			break;

		case TYPE_COL4U:
			col3.setValue(self->childGetValue("color_swatch"));
			col4U.setVecScaleClamp(col3);
			col4U.mV[VALPHA] = self->childGetValue("val_spinner_4").asInteger();
			controlp->setValue(col4U.getValue());
			break;

		default:
			break;
	}
}

// static
void LLFloaterDebugSettings::onClickDefault(void* user_data)
{
//MK
	// Do not allow Reset To Default when under @setdebug (that could give
	// funny results)
	if (gRLenabled && gRLInterface.mContainsSetdebug)
	{
		return;
	}
//mk

	LLFloaterDebugSettings* self = (LLFloaterDebugSettings*)user_data;
	if (!self) return;

	LLComboBox* settings_combo = self->mComboNames;
	LLControlVariable* controlp;
	controlp = (LLControlVariable*)settings_combo->getCurrentUserdata();
	if (controlp)
	{
		controlp->resetToDefault(true);
		self->updateControl(controlp);
	}
}

// We have switched controls, or doing per-frame update, so update spinners,
// etc.
void LLFloaterDebugSettings::updateControl(LLControlVariable* controlp)
{
	LLSpinCtrl* spinner1 = getChild<LLSpinCtrl>("val_spinner_1");
	LLSpinCtrl* spinner2 = getChild<LLSpinCtrl>("val_spinner_2");
	LLSpinCtrl* spinner3 = getChild<LLSpinCtrl>("val_spinner_3");
	LLSpinCtrl* spinner4 = getChild<LLSpinCtrl>("val_spinner_4");
	LLColorSwatchCtrl* color_swatch =
		getChild<LLColorSwatchCtrl>("color_swatch");

	spinner1->setVisible(false);
	spinner2->setVisible(false);
	spinner3->setVisible(false);
	spinner4->setVisible(false);
	color_swatch->setVisible(false);
	childSetVisible("val_text", false);
	mComment->setText(LLStringUtil::null);

	if (controlp)
	{
		eControlType type = controlp->type();

		// Hide combo box only for non booleans, otherwise this will result in
		// the combo box closing every frame
		childSetVisible("boolean_combo", type == TYPE_BOOLEAN);

		mComment->setText(controlp->getComment());
		spinner1->setMaxValue(F32_MAX);
		spinner2->setMaxValue(F32_MAX);
		spinner3->setMaxValue(F32_MAX);
		spinner4->setMaxValue(F32_MAX);
		spinner1->setMinValue(-F32_MAX);
		spinner2->setMinValue(-F32_MAX);
		spinner3->setMinValue(-F32_MAX);
		spinner4->setMinValue(-F32_MAX);
		if (!spinner1->hasFocus())
		{
			spinner1->setIncrement(0.1f);
		}
		if (!spinner2->hasFocus())
		{
			spinner2->setIncrement(0.1f);
		}
		if (!spinner3->hasFocus())
		{
			spinner3->setIncrement(0.1f);
		}
		if (!spinner4->hasFocus())
		{
			spinner4->setIncrement(0.1f);
		}

		LLSD sd = controlp->getValue();
		switch (type)
		{
			case TYPE_U32:
				spinner1->setVisible(true);
				spinner1->setLabel("value"); // Debug, don't translate
				if (!spinner1->hasFocus())
				{
					spinner1->setValue(sd);
					spinner1->setMinValue((F32)U32_MIN);
					spinner1->setMaxValue((F32)U32_MAX);
					spinner1->setIncrement(1.f);
					spinner1->setPrecision(0);
				}
				break;

			case TYPE_S32:
				spinner1->setVisible(true);
				spinner1->setLabel("value"); // Debug, don't translate
				if (!spinner1->hasFocus())
				{
					spinner1->setValue(sd);
					spinner1->setMinValue((F32)S32_MIN);
					spinner1->setMaxValue((F32)S32_MAX);
					spinner1->setIncrement(1.f);
					spinner1->setPrecision(0);
				}
				break;

			case TYPE_F32:
				spinner1->setVisible(true);
				spinner1->setLabel("value"); // Debug, don't translate
				if (!spinner1->hasFocus())
				{
					spinner1->setPrecision(5);
					spinner1->setValue(sd);
				}
				break;

			case TYPE_BOOLEAN:
				if (!childHasFocus("boolean_combo"))
				{
					if (sd.asBoolean())
					{
						childSetValue("boolean_combo", LLSD("true"));
					}
					else
					{
						childSetValue("boolean_combo", LLSD(""));
					}
				}
				break;

			case TYPE_STRING:
				childSetVisible("val_text", true);
				if (!childHasFocus("val_text"))
				{
					childSetValue("val_text", sd);
				}
				break;

			case TYPE_VEC3:
			{
				LLVector3 v;
				v.setValue(sd);
				spinner1->setVisible(true);
				spinner1->setLabel("X");
				spinner2->setVisible(true);
				spinner2->setLabel("Y");
				spinner3->setVisible(true);
				spinner3->setLabel("Z");
				if (!spinner1->hasFocus())
				{
					spinner1->setPrecision(3);
					spinner1->setValue(v[VX]);
				}
				if (!spinner2->hasFocus())
				{
					spinner2->setPrecision(3);
					spinner2->setValue(v[VY]);
				}
				if (!spinner3->hasFocus())
				{
					spinner3->setPrecision(3);
					spinner3->setValue(v[VZ]);
				}
				break;
			}

			case TYPE_VEC3D:
			{
				LLVector3d v;
				v.setValue(sd);
				spinner1->setVisible(true);
				spinner1->setLabel("X");
				spinner2->setVisible(true);
				spinner2->setLabel("Y");
				spinner3->setVisible(true);
				spinner3->setLabel("Z");
				if (!spinner1->hasFocus())
				{
					spinner1->setPrecision(3);
					spinner1->setValue(v[VX]);
				}
				if (!spinner2->hasFocus())
				{
					spinner2->setPrecision(3);
					spinner2->setValue(v[VY]);
				}
				if (!spinner3->hasFocus())
				{
					spinner3->setPrecision(3);
					spinner3->setValue(v[VZ]);
				}
				break;
			}

			case TYPE_RECT:
			{
				LLRect r;
				r.setValue(sd);
				spinner1->setVisible(true);
				spinner1->setLabel("Left");
				spinner2->setVisible(true);
				spinner2->setLabel("Right");
				spinner3->setVisible(true);
				spinner3->setLabel("Bottom");
				spinner4->setVisible(true);
				spinner4->setLabel("Top");
				if (!spinner1->hasFocus())
				{
					spinner1->setPrecision(0);
					spinner1->setValue(r.mLeft);
				}
				if (!spinner2->hasFocus())
				{
					spinner2->setPrecision(0);
					spinner2->setValue(r.mRight);
				}
				if (!spinner3->hasFocus())
				{
					spinner3->setPrecision(0);
					spinner3->setValue(r.mBottom);
				}
				if (!spinner4->hasFocus())
				{
					spinner4->setPrecision(0);
					spinner4->setValue(r.mTop);
				}

				spinner1->setMinValue((F32)S32_MIN);
				spinner1->setMaxValue((F32)S32_MAX);
				spinner1->setIncrement(1.f);

				spinner2->setMinValue((F32)S32_MIN);
				spinner2->setMaxValue((F32)S32_MAX);
				spinner2->setIncrement(1.f);

				spinner3->setMinValue((F32)S32_MIN);
				spinner3->setMaxValue((F32)S32_MAX);
				spinner3->setIncrement(1.f);

				spinner4->setMinValue((F32)S32_MIN);
				spinner4->setMaxValue((F32)S32_MAX);
				spinner4->setIncrement(1.f);
				break;
			}

			case TYPE_COL4:
			{
				LLColor4 clr;
				clr.setValue(sd);
				color_swatch->setVisible(true);
				// only set if changed so color picker doesn't update
				if (clr != LLColor4(color_swatch->getValue()))
				{
					color_swatch->set(LLColor4(sd), true, false);
				}
				spinner4->setVisible(true);
				spinner4->setLabel("Alpha");
				if (!spinner4->hasFocus())
				{
					spinner4->setPrecision(3);
					spinner4->setMinValue(0.0);
					spinner4->setMaxValue(1.f);
					spinner4->setValue(clr.mV[VALPHA]);
				}
				break;
			}

			case TYPE_COL3:
			{
				LLColor3 clr;
				clr.setValue(sd);
				color_swatch->setVisible(true);
				color_swatch->setValue(sd);
				break;
			}

			case TYPE_COL4U:
			{
				LLColor4U clr;
				clr.setValue(sd);
				color_swatch->setVisible(true);
				if (LLColor4(clr) != LLColor4(color_swatch->getValue()))
				{
					color_swatch->set(LLColor4(clr), true, false);
				}
				spinner4->setVisible(true);
				spinner4->setLabel("Alpha");
				if (!spinner4->hasFocus())
				{
					spinner4->setPrecision(0);
					spinner4->setValue(clr.mV[VALPHA]);
				}

				spinner4->setMinValue(0);
				spinner4->setMaxValue(255);
				spinner4->setIncrement(1.f);

				break;
			}

			default:
				mComment->setText("unknown");
				break;
		}
	}
}
