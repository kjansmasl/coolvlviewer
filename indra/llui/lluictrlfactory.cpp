/**
 * @file lluictrlfactory.cpp
 * @brief Factory class for creating UI controls
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <fstream>

#include "boost/tokenizer.hpp"

#include "lluictrlfactory.h"

#include "llcolor4.h"
#include "llcontrol.h"
#include "lldir.h"
#include "llmenugl.h"

const char XML_HEADER[] = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?>\n";

std::vector<std::string> LLUICtrlFactory::sXUIPaths;

static const std::string LL_UI_CTRL_LOCATE_TAG = "locate";
static const std::string LL_PAD_TAG = "pad";

// UI Ctrl class for padding
class LLUICtrlLocate : public LLUICtrl
{
public:
	LLUICtrlLocate()
	:	LLUICtrl("locate", LLRect(0, 0, 0, 0), false, NULL, NULL)
	{
		setTabStop(false);
	}

	virtual void draw()
	{
	}

	virtual LLXMLNodePtr getXML(bool save_children = true) const
	{
		LLXMLNodePtr node = LLUICtrl::getXML();
		node->setName(LL_UI_CTRL_LOCATE_TAG);
		return node;
	}

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory)
	{
		std::string name = LL_PAD_TAG;
		node->getAttributeString("name", name);

		LLUICtrlLocate* new_ctrl = new LLUICtrlLocate();
		new_ctrl->setName(name);
		new_ctrl->initFromXML(node, parent);
		return new_ctrl;
	}
};

static LLRegisterWidget<LLUICtrlLocate> r29(LL_UI_CTRL_LOCATE_TAG);
static LLRegisterWidget<LLUICtrlLocate> r30(LL_PAD_TAG);

LLUICtrlFactory::LLUICtrlFactory()
:	mDummyPanel(NULL)
{
	setupPaths();
}

LLUICtrlFactory::~LLUICtrlFactory()
{
	delete mDummyPanel;
	mDummyPanel = NULL;
}

void LLUICtrlFactory::setupPaths()
{
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_SKINS,
														  "paths.xml");

	LLXMLNodePtr root;
	bool success  = LLXMLNode::parseFile(filename, root, NULL);
	sXUIPaths.clear();

	if (success)
	{
		LLXMLNodePtr path;

		for (path = root->getFirstChild(); path.notNull();
			 path = path->getNextSibling())
		{
			LLUIString path_val_ui(path->getValue());
			std::string language = LLUI::getLanguage();
			path_val_ui.setArg("[LANGUAGE]", language);

			if (std::find(sXUIPaths.begin(), sXUIPaths.end(),
						  path_val_ui.getString()) == sXUIPaths.end())
			{
				sXUIPaths.emplace_back(path_val_ui.getString());
			}
		}
	}
	else // Parsing failed
	{
		llwarns << "XUI::config file unable to open: " << filename << llendl;
		sXUIPaths.emplace_back("xui" LL_DIR_DELIM_STR "en-us");
	}
}

//static
const std::vector<std::string>& LLUICtrlFactory::getXUIPaths()
{
	return sXUIPaths;
}

bool LLUICtrlFactory::getLayeredXMLNode(const std::string& xui_filename,
										LLXMLNodePtr& root)
{
	std::string full_filename =
		gDirUtilp->findSkinnedFilename(sXUIPaths.front(), xui_filename);
	if (full_filename.empty())
	{
		// Try filename as passed in since sometimes we load an xml file from a
		// user-supplied path
		if (LLFile::exists(xui_filename))
		{
			full_filename = xui_filename;
		}
		else
		{
			llwarns << "Could not find UI description file: "
					<< sXUIPaths.front() + "/" + xui_filename
					<< llendl;
			return false;
		}
	}

	if (!LLXMLNode::parseFile(full_filename, root, NULL))
	{
		llwarns << "Problem reading UI description file: " << full_filename
				<< llendl;
		return false;
	}

	LLXMLNodePtr upd_root;
	std::string layer_filename, node_name, upd_name;
	for (std::vector<std::string>::const_iterator it = sXUIPaths.begin(),
												  end = sXUIPaths.end();
		 it != end; ++it)
	{
		// Skip the first path to only consider overrides
		if (it == sXUIPaths.begin()) continue;

		layer_filename = gDirUtilp->findSkinnedFilename(*it, xui_filename);
		if (layer_filename.empty())
		{
			// No localized version of this file, that's ok, keep looking
			continue;
		}

		if (!LLXMLNode::parseFile(layer_filename, upd_root, NULL))
		{
			llwarns << "Problem reading localized UI description file: "
					<< *it + LL_DIR_DELIM_STR + xui_filename << llendl;
			return false;
		}

		upd_root->getAttributeString("name", upd_name);
		root->getAttributeString("name", node_name);

		if (upd_name == node_name)
		{
			LLXMLNode::updateNode(root, upd_root);
		}
	}

	return true;
}

bool LLUICtrlFactory::buildFloater(LLFloater* floaterp,
								   const std::string& filename,
								   const LLCallbackMap::map_t* factory_map,
								   bool open)
{
	LLXMLNodePtr root;

	if (!getLayeredXMLNode(filename, root))
	{
		return false;
	}

	// root must be called "floater"
	if (!(root->hasName("floater") || root->hasName("multi_floater")))
	{
		llwarns << "Root node should be named floater in: " << filename
				<< llendl;
		return false;
	}

	if (factory_map)
	{
		mFactoryStack.push_front(factory_map);
	}

	floaterp->initFloaterXML(root, NULL, this, open);

	if (LLUI::sShowXUINames)
	{
		floaterp->setToolTip(filename);
	}

	if (factory_map)
	{
		mFactoryStack.pop_front();
	}

	LLHandle<LLFloater> handle = floaterp->getHandle();
	mBuiltFloaters[handle] = filename;

	return true;
}

S32 LLUICtrlFactory::saveToXML(LLView* viewp, const std::string& filename)
{
	llofstream out(filename.c_str());
	if (!out.is_open())
	{
		llwarns << "Unable to open " << filename << " for output." << llendl;
		return 1;
	}

	out << XML_HEADER;

	LLXMLNodePtr xml_node = viewp->getXML();

	xml_node->writeToOstream(out);

	out.close();
	return 0;
}

bool LLUICtrlFactory::buildPanel(LLPanel* panelp, const std::string& filename,
								 const LLCallbackMap::map_t* factory_map)
{
	LLXMLNodePtr root;
	if (!getLayeredXMLNode(filename, root))
	{
		return false;
	}

	// root must be called "panel"
	if (!root->hasName("panel"))
	{
		llwarns << "Root node should be named panel in : " << filename
				<< llendl;
		return false;
	}

	if (factory_map)
	{
		mFactoryStack.push_front(factory_map);
	}

	bool result = panelp->initPanelXML(root, NULL, this);

	if (LLUI::sShowXUINames)
	{
		panelp->setToolTip(filename);
	}

	LLHandle<LLPanel> handle = panelp->getHandle();
	mBuiltPanels[handle] = filename;

	if (factory_map)
	{
		mFactoryStack.pop_front();
	}

	return result;
}

LLMenuGL* LLUICtrlFactory::buildMenu(const std::string& filename,
									 LLView* parentp)
{
	LLXMLNodePtr root;
	LLMenuGL* menu;

	if (!getLayeredXMLNode(filename, root))
	{
		return NULL;
	}

	// root must be called "menu_bar" or "menu"
	if (!root->hasName("menu_bar") && !root->hasName("menu"))
	{
		llwarns << "Root node should be named menu bar or menu in: "
				<< filename << llendl;
		return NULL;
	}

	if (root->hasName("menu"))
	{
		menu = (LLMenuGL*)LLMenuGL::fromXML(root, parentp, this);
	}
	else
	{
		menu = (LLMenuGL*)LLMenuBarGL::fromXML(root, parentp, this);
	}

	if (LLUI::sShowXUINames)
	{
		menu->setToolTip(filename);
	}

    return menu;
}

LLPieMenu* LLUICtrlFactory::buildPieMenu(const std::string& filename,
										 LLView* parentp)
{
	LLXMLNodePtr root;

	if (!getLayeredXMLNode(filename, root))
	{
		return NULL;
	}

	// root must be called "pie_menu"
	if (!root->hasName(LL_PIE_MENU_TAG))
	{
		llwarns << "Root node should be named " << LL_PIE_MENU_TAG << " in: "
				<< filename << llendl;
		return NULL;
	}

	std::string name = "menu";
	root->getAttributeString("name", name);

	LLPieMenu* menu = new LLPieMenu(name);
	parentp->addChild(menu);
	menu->initXML(root, parentp, this);

	if (LLUI::sShowXUINames)
	{
		menu->setToolTip(filename);
	}

	return menu;
}

void LLUICtrlFactory::rebuild()
{
	built_panel_t::iterator built_panel_it;
	for (built_panel_it = mBuiltPanels.begin();
		 built_panel_it != mBuiltPanels.end(); ++built_panel_it)
	{
		std::string filename = built_panel_it->second;
		LLPanel* panelp = built_panel_it->first.get();
		if (!panelp)
		{
			continue;
		}
		llinfos << "Rebuilding UI panel " << panelp->getName() << " from "
				<< filename << llendl;
		bool visible = panelp->getVisible();
		panelp->setVisible(false);
		panelp->setFocus(false);
		panelp->deleteAllChildren();

		buildPanel(panelp, filename.c_str(), &panelp->getFactoryMap());
		panelp->setVisible(visible);
	}

	built_floater_t::iterator built_floater_it;
	for (built_floater_it = mBuiltFloaters.begin();
		 built_floater_it != mBuiltFloaters.end(); ++built_floater_it)
	{
		LLFloater* floaterp = built_floater_it->first.get();
		if (!floaterp)
		{
			continue;
		}
		std::string filename = built_floater_it->second;
		llinfos << "Rebuilding UI floater " << floaterp->getName() << " from "
				<< filename << llendl;
		bool visible = floaterp->getVisible();
		floaterp->setVisible(false);
		floaterp->setFocus(false);
		floaterp->deleteAllChildren();

		gFloaterViewp->removeChild(floaterp);
		buildFloater(floaterp, filename, &floaterp->getFactoryMap());
		floaterp->setVisible(visible);
	}
}

LLView* LLUICtrlFactory::createCtrlWidget(LLPanel* parent, LLXMLNodePtr node)
{
	std::string ctrl_type = node->getName()->mString;
	LLStringUtil::toLower(ctrl_type);

	LLWidgetClassRegistry::factory_func_t func  =
		LLWidgetClassRegistry::getInstance()->getCreatorFunc(ctrl_type);
	if (!func)
	{
		llwarns << "Invalid control type '" << ctrl_type << "' - Parent: "
				<< (parent ? parent->getName() : "none") << llendl;
		return NULL;
	}

	if (!parent)
	{
		if (!mDummyPanel)
		{
			mDummyPanel = new LLPanel;
		}
		parent = mDummyPanel;
	}

	LLView* ctrl = func(node, parent, this);
	return ctrl;
}

LLView* LLUICtrlFactory::createWidget(LLPanel* parent, LLXMLNodePtr node)
{
	LLView* view = createCtrlWidget(parent, node);

	S32 tab_group = parent->getLastTabGroup();
	node->getAttributeS32("tab_group", tab_group);

	if (view)
	{
		parent->addChild(view, tab_group);
	}

	return view;
}

LLPanel* LLUICtrlFactory::createFactoryPanel(const std::string& name)
{
	std::deque<const LLCallbackMap::map_t*>::iterator itor;
	for (itor = mFactoryStack.begin(); itor != mFactoryStack.end(); ++itor)
	{
		const LLCallbackMap::map_t* factory_map = *itor;

		// Look up this panel's name in the map.
		LLCallbackMap::map_const_iter_t iter = factory_map->find(name);
		if (iter != factory_map->end())
		{
			// Use the factory to create the panel, instead of using a default
			// LLPanel.
			LLPanel* ret = (LLPanel*)iter->second.mCallback(iter->second.mData);
			return ret;
		}
	}
	return NULL;
}

//static
bool LLUICtrlFactory::getAttributeColor(LLXMLNodePtr node,
										const std::string& name,
										LLColor4& color)
{
	std::string colorstring;
	bool res = node->getAttributeString(name.c_str(), colorstring);
	if (res && LLUI::sColorsGroup)
	{
		if (LLUI::sColorsGroup->controlExists(colorstring.c_str()))
		{
			color.set(LLUI::sColorsGroup->getColor(colorstring.c_str()));
		}
		else
		{
			res = false;
		}
	}
	if (!res)
	{
		res = LLColor4::parseColor(colorstring, &color);
	}
	if (!res)
	{
		res = node->getAttributeColor(name.c_str(), color);
	}

	return res;
}
