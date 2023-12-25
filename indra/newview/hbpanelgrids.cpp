/** 
 * @file hbpanelgrids.cpp
 * @author Henri Beauchamp
 * @brief Grid parameters configuration panel
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 * 
 * Copyright (c) 2011, Henri Beauchamp.
 * Note: XML parser code borrowed from Hippo Viewer (c) unknown author
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

#include <regex>

#include "hbpanelgrids.h"

#include "llcheckboxctrl.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llgridmanager.h"
#include "llstartup.h"

class HBPanelGridsImpl final : public LLPanel
{
protected:
	LOG_CLASS(HBPanelGridsImpl);

public:
	HBPanelGridsImpl();
	~HBPanelGridsImpl() override;

	void draw() override;

	void apply();
	void cancel();

	enum XmlState 
	{
		XML_VOID,
		XML_GRIDNAME,
		XML_GRIDNICK,
		XML_LOGINURI,
		XML_HELPERURI,
		XML_LOGINPAGE,
		XML_WEBSITE,
		XML_SUPPORT,
		XML_ACCOUNT,
		XML_PASSWORD,
	};
	XmlState mXmlState;

	void getParams();
	void clearParams(bool clear_name = true);
	void updateGridParameters(std::string& result);
	void copyParams();
	void saveParams();
	void deleteGrid();
	void addGrid();

	void setDirty()							{ mIsDirty = mIsDirtyGrid = true; }
	void setQueryActive(bool active)		{ mQueryActive = active; mIsDirty = true; }

private:
	static void getParamsCoro(std::string uri);

	static void onXmlElementStart(void* data, const XML_Char* name,
								  const XML_Char** atts);
	static void onXmlElementEnd(void* data, const XML_Char* name);
	static void onXmlCharacterData(void* data, const XML_Char* s, int len);

	static void onClickGetParams(void* data);
	static void onClickClearParams(void* data);
	static void onClickUpdateGrid(void* data);
	static void onClickDeleteGrid(void* data);
	static void onClickAddGrid(void* data);

	static void onEditorKeystroke(LLLineEditor*, void* data);
	static void onNameEditorKeystroke(LLLineEditor* caller, void* data);
	static void onCommitCheckBoxLoginURI(LLUICtrl* ctrl, void* data);
	static void onCommitRadioPreferredName(LLUICtrl* ctrl, void* data);
	static void onSelectGrid(LLUICtrl* ctrl, void* data);

private:
	LLScrollListCtrl*			mGridsScrollList;

	bool						mIsDirty;
	bool						mIsDirtyList;
	bool						mIsDirtyGrid;
	bool						mGridNeedsUpdate;
	bool						mQueryActive;
	bool						mListChanged;

	std::string					mGridDomain;
	std::string					mGridCustomName;
	std::string					mGridName;
	std::string					mGridNick;
	std::string					mEnteredLoginURI;
	std::string					mLoginURI;
	std::string					mHelperURI;
	std::string					mLoginPage;
	std::string					mWebsiteURL;
	std::string					mSupportURL;
	std::string					mAccountURL;
	std::string					mPasswordURL;

	LLSD						mSavedGridsList;
	static LLSD					sGridsList;

	static HBPanelGridsImpl*	sInstance;
};

HBPanelGridsImpl* HBPanelGridsImpl::sInstance = NULL;
LLSD HBPanelGridsImpl::sGridsList;

HBPanelGridsImpl::HBPanelGridsImpl()
:	LLPanel(std::string("Grids parameters")),
	mIsDirty(true),
	mIsDirtyList(true),
	mQueryActive(false),
	mListChanged(false),
	mIsDirtyGrid(false),
	mGridNeedsUpdate(false)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_grids.xml");
	sInstance = this;

	if (sGridsList.beginMap() == sGridsList.endMap())
	{
		const LLSD& grids = LLGridManager::getInstance()->getGridsList();
		for (LLSD::map_const_iterator it = grids.beginMap(),
									  end = grids.endMap();
			 it != end; ++it)
		{
			LLSD::String key_name = it->first;
			LLSD grid_array = it->second;
			if (key_name == "grids" && grid_array.isArray())
			{
				for (size_t i = 0; i < grid_array.size(); ++i)
				{
					LLSD gmap = grid_array[i];
					if (gmap.has("can_edit") &&
						gmap["can_edit"].asString() != "never")
					{
						sGridsList["grids"].append(gmap);
						LL_DEBUGS("GetGridParameters") << "Retained grid: "
													   << gmap.get("name")
													   << LL_ENDL;
					}
					else
					{
						LL_DEBUGS("GetGridParameters") << "Rejected non-editable grid: "
													   << gmap.get("name")
													   << LL_ENDL;
					}
				}
			}
		}
	}

	mSavedGridsList = sGridsList;

	mGridsScrollList = getChild<LLScrollListCtrl>("grid_selector");
	mGridsScrollList->setCommitOnSelectionChange(true);
	mGridsScrollList->setCommitCallback(onSelectGrid);
	mGridsScrollList->setCallbackUserData(this);

	childSetAction("update_button", onClickUpdateGrid, this);
	childSetAction("delete_button", onClickDeleteGrid, this);
	childSetAction("add_button", onClickAddGrid, this);

	childSetAction("get_param_button", onClickGetParams, this);
	childSetAction("clear_param_button", onClickClearParams, this);

	LLLineEditor* editor = getChild<LLLineEditor>("login_uri_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("grid_name_editor");
	editor->setKeystrokeCallback(onNameEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("helper_uri_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("login_page_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("website_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("support_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("new_account_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	editor = getChild<LLLineEditor>("forgotten_password_editor");
	editor->setKeystrokeCallback(onEditorKeystroke);
	editor->setCallbackUserData(this);

	childSetCommitCallback("retrieved_loginuri_check",
						   onCommitCheckBoxLoginURI, this);
	childSetCommitCallback("prefer_nickname_radio",
						   onCommitRadioPreferredName, this);
}

HBPanelGridsImpl::~HBPanelGridsImpl()
{
	sInstance = NULL;
}

//virtual
void HBPanelGridsImpl::draw()
{
	if (mIsDirty)
	{
		// Grids list
		if (mIsDirtyList)
		{
			S32 old_count = mGridsScrollList->getItemCount();
			S32 scrollpos = mGridsScrollList->getScrollPos();
			S32 selected = mGridsScrollList->getFirstSelectedIndex();
			mGridsScrollList->deleteAllItems();

			for (LLSD::map_const_iterator it = sGridsList.beginMap(),
										  end = sGridsList.endMap();
				 it != end; ++it)
			{
				LLSD::String key_name = it->first;
				LLSD grid_array = it->second;
				if (key_name == "grids" && grid_array.isArray())
				{
					for (size_t i = 0; i < grid_array.size(); ++i)
					{
						LLSD gmap = grid_array[i];
						LLSD element;
						std::string style = "NORMAL";
						std::string grid_id = gmap["name"].asString();
						if (gmap.has("can_edit") &&
							gmap["can_edit"].asString() == "false")
						{
							style = "BOLD";
							grid_id = "@@|" + grid_id;
						}
						element["id"] = grid_id;
						element["columns"][0]["value"] = gmap["label"].asString();
						element["columns"][0]["type"] = "text";
						element["columns"][0]["font"] = "SANSSERIF";
						element["columns"][0]["font-style"] = style;
						mGridsScrollList->addElement(element);
					}
				}
			}

			S32 new_count = mGridsScrollList->getItemCount();
			if (old_count > new_count)
			{
				// A grid was just deleted
				if (selected > 0)
				{
					scrollpos = --selected;
				}
				else
				{
					scrollpos = selected = 0;
				}
			}
			else if (old_count < new_count &&
					 // count == 0 when first initializing the list
					 old_count > 0)
			{
				// An item was just added. Let's select it and scroll to it.
				selected = scrollpos = new_count - 1;
			}
			mGridsScrollList->setScrollPos(scrollpos);
			if (selected >= 0)
			{
				mGridsScrollList->selectNthItem(selected);
			}
			mIsDirtyList = false;
		}
		mGridsScrollList->setEnabled(!mQueryActive);

		// Enable/disable the various UI elements as appropriate

		bool uri_ok = !childGetValue("login_uri_editor").asString().empty();
		bool name_ok = !childGetValue("grid_name_editor").asString().empty();
		bool grid_ok = !mIsDirtyList &&
					   mGridsScrollList->getFirstSelected() != NULL;
		mGridNeedsUpdate = mIsDirtyGrid && !mQueryActive && uri_ok &&
						   name_ok && grid_ok;
		childSetEnabled("update_button", mGridNeedsUpdate);
		if (grid_ok)
		{
			grid_ok = mGridsScrollList->getValue().asString().find("@@|") ==
				std::string::npos;
		}
		childSetEnabled("delete_button", !mQueryActive && grid_ok);
		childSetEnabled("add_button", !mQueryActive && uri_ok && name_ok);
		childSetEnabled("get_param_button", !mQueryActive && uri_ok);
		childSetEnabled("clear_param_button", !mQueryActive);

		childSetVisible("retreiving", mQueryActive);
		if (mQueryActive)
		{
			childSetVisible("domain", false);
		}
		else if (!mGridDomain.empty())
		{
			LLTextBox* domain_text = getChild<LLTextBox>("domain");
			domain_text->setTextArg("[DOMAIN]", mGridDomain);
			domain_text->setVisible(true);
		}
		else
		{
			childSetVisible("domain", false);
		}

		// Updates done
		mIsDirty = false;
	}
	LLPanel::draw();
}

void HBPanelGridsImpl::getParams()
{
	mEnteredLoginURI = childGetValue("login_uri_editor").asString();
	if (mEnteredLoginURI.empty())
	{
		gNotifications.add("MandatoryLoginUri");
		return;
	}
	clearParams(false);
	gCoros.launch("HBPanelGridsImpl::getParamsCoro",
			   	  boost::bind(&HBPanelGridsImpl::getParamsCoro,
							  mEnteredLoginURI));
}

//static
void HBPanelGridsImpl::getParamsCoro(std::string uri)
{
	if (!sInstance) return;	// Paranoia
	sInstance->setQueryActive(true);

	std::string url = uri;
	if (uri.compare(url.length() - 1, 1, "/") != 0)
	{
		url += '/';
	}
	url += "get_grid_info";
	llinfos << "Fetching grid parameters from: " << url << llendl;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getParamsCoro");
	LLSD result = adapter.getRawAndSuspend(url);

	if (!sInstance)
	{
		llwarns << "Panel closed, grid parameters response from " << url
				<< " discarded." << llendl;
		return;
	}

	sInstance->setQueryActive(false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		LLSD args;
		args["URI"] = uri;
		args["STATUS"] = llformat("%d", status.getType());
		args["REASON"] = status.toString();
		gNotifications.add("GetGridParametersFailure", args);
		return;
	}

	const LLSD::Binary& raw =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
	S32 size = raw.size();
	if (size <= 0)
	{
		llwarns << "Empty parameters received from: " << url << llendl;
		return;
	}

	std::string parameters((char*)raw.data(), size);
	LL_DEBUGS("GetGridParameters") << "\n" << parameters << LL_ENDL;
	sInstance->updateGridParameters(parameters);
}

void HBPanelGridsImpl::clearParams(bool clear_name)
{
	if (clear_name)
	{
		childSetValue("grid_name_editor", "");
	}
	childSetValue("helper_uri_editor", "");
	childSetValue("login_page_editor", "");
	childSetValue("website_editor", "");
	childSetValue("new_account_editor", "");
	childSetValue("support_editor", "");
	childSetValue("forgotten_password_editor", "");

	mGridDomain = "";
	mIsDirty = mIsDirtyGrid = true;
}

//static
void HBPanelGridsImpl::onXmlElementStart(void* data, const XML_Char* name,
										 const XML_Char** atts)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (stricmp(name, "gridnick") == 0)
	{
		self->mXmlState = XML_GRIDNICK;
	}
	else if (stricmp(name, "gridname") == 0)
	{
		self->mXmlState = XML_GRIDNAME;
	}
	else if (stricmp(name, "loginuri") == 0 || stricmp(name, "login") == 0)
	{
		self->mXmlState = XML_LOGINURI;
	}
	else if (stricmp(name, "helperuri") == 0 || stricmp(name, "economy") == 0)
	{
		self->mXmlState = XML_HELPERURI;
	}
	else if (stricmp(name, "loginpage") == 0 || stricmp(name, "welcome") == 0)
	{
		self->mXmlState = XML_LOGINPAGE;
	}
	else if (stricmp(name, "website") == 0 || stricmp(name, "about") == 0)
	{
		self->mXmlState = XML_WEBSITE;
	}
	else if (stricmp(name, "support") == 0 || stricmp(name, "help") == 0)
	{
		self->mXmlState = XML_SUPPORT;
	}
	else if (stricmp(name, "account") == 0 || stricmp(name, "register") == 0)
	{
		self->mXmlState = XML_ACCOUNT;
	}
	else if (stricmp(name, "password") == 0)
	{
		self->mXmlState = XML_PASSWORD;
	}
}

//static
void HBPanelGridsImpl::onXmlElementEnd(void* data, const XML_Char* name)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->mXmlState = XML_VOID;
	}
}

//static
void HBPanelGridsImpl::onXmlCharacterData(void* data, const XML_Char* s,
										  int len)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	switch (self->mXmlState) 
	{
		case XML_GRIDNAME:	self->mGridName.assign(s, len);		break;
		case XML_GRIDNICK:	self->mGridNick.assign(s, len);		break;
		case XML_LOGINURI:	self->mLoginURI.assign(s, len);		break;
		case XML_HELPERURI:	self->mHelperURI.assign(s, len);	break;
		case XML_LOGINPAGE:	self->mLoginPage.assign(s, len);	break;
		case XML_WEBSITE:	self->mWebsiteURL.assign(s, len);	break;
		case XML_SUPPORT:	self->mSupportURL.assign(s, len);	break;
		case XML_ACCOUNT:	self->mAccountURL.assign(s, len);	break;
		case XML_PASSWORD:	self->mPasswordURL.assign(s, len);  break;
		case XML_VOID:
		default:
			break;
	}
}

void HBPanelGridsImpl::updateGridParameters(std::string& result)
{
	mGridName.clear();
	mGridNick.clear();
	mLoginURI.clear();
	mHelperURI.clear();
	mLoginPage.clear();
	mWebsiteURL.clear();
	mSupportURL.clear();
	mAccountURL.clear();
	mPasswordURL.clear();

	XML_Parser parser = XML_ParserCreate(0);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, onXmlElementStart, onXmlElementEnd);
	XML_SetCharacterDataHandler(parser, onXmlCharacterData);
	mXmlState = XML_VOID;
	if (!XML_Parse(parser, result.data(), result.size(), true)) 
	{
		llwarns << "XML Parse Error: "
				<< XML_ErrorString(XML_GetErrorCode(parser)) << llendl;
	}
	XML_ParserFree(parser);

	if (mGridName.empty() && !mGridNick.empty())
	{
		mGridName = mGridNick;
	}
	if (mGridCustomName.empty())
	{
		mGridCustomName = mGridName;
	}
	if (mGridName.empty())
	{
		mGridName = mGridCustomName;
	}
	if (mGridNick.empty())
	{
		mGridNick = mGridName;
	}
	S32 choice = childGetValue("prefer_nickname_radio").asInteger();
	std::string name;
	switch (choice)
	{
		case 1:
			name = mGridName;
			break;
		case 2:
			name = mGridNick;
			break;
		default:
			name = mGridCustomName;
	}
	childSetValue("grid_name_editor", name);

	if (mLoginURI.empty())
	{
		mLoginURI = mEnteredLoginURI;
	}
	std::string login_uri =
		childGetValue("retrieved_loginuri_check").asBoolean() ? mLoginURI
															  : mEnteredLoginURI;
	childSetValue("login_uri_editor", login_uri);

	childSetValue("helper_uri_editor", mHelperURI);
	childSetValue("login_page_editor", mLoginPage);
	childSetValue("website_editor", mWebsiteURL);
	childSetValue("new_account_editor", mAccountURL);
	childSetValue("support_editor", mSupportURL);
	childSetValue("forgotten_password_editor", mPasswordURL);

	mIsDirty = mIsDirtyGrid = true;
}

void HBPanelGridsImpl::copyParams()
{
	mGridDomain = mGridsScrollList->getValue().asString();
	if (mGridDomain.empty()) return;
	if (mGridDomain.find("@@|") == 0)
	{
		mGridDomain = mGridDomain.substr(3);
	}

	EGridInfo i = LLGridManager::getInstance()->gridIndexInList(sGridsList,
																mGridDomain);
	if (i != -1)
	{
		mGridCustomName = mGridName = mGridNick = sGridsList["grids"][i].get("label").asString();
		childSetValue("grid_name_editor", mGridCustomName);

		mLoginURI = mEnteredLoginURI = sGridsList["grids"][i].get("login_uri").asString();
		childSetValue("login_uri_editor", mLoginURI);

		mHelperURI = sGridsList["grids"][i].get("helper_uri").asString();
		childSetValue("helper_uri_editor", mHelperURI);

		mLoginPage = sGridsList["grids"][i].get("login_page").asString();
		childSetValue("login_page_editor", mLoginPage);

		mWebsiteURL = sGridsList["grids"][i].get("website_url").asString();
		childSetValue("website_editor", mWebsiteURL);

		mSupportURL = sGridsList["grids"][i].get("support_url").asString();
		childSetValue("support_editor", mSupportURL);

		mAccountURL = sGridsList["grids"][i].get("register_url").asString();
		childSetValue("new_account_editor", mAccountURL);

		mPasswordURL = sGridsList["grids"][i].get("password_url").asString();
		childSetValue("forgotten_password_editor", mPasswordURL);

		mIsDirty = true;
		mIsDirtyGrid = mGridNeedsUpdate = false;
	}
}

void HBPanelGridsImpl::saveParams()
{
	mGridDomain = mGridsScrollList->getValue().asString();
	if (mGridDomain.empty()) return;
	if (mGridDomain.find("@@|") == 0)
	{
		mGridDomain = mGridDomain.substr(3);
	}

	EGridInfo i = LLGridManager::getInstance()->gridIndexInList(sGridsList,
																mGridDomain);
	if (i != -1)
	{
		std::string name = childGetValue("grid_name_editor").asString();
		LLStringUtil::trim(name);
		if (name.empty())
		{
			gNotifications.add("MandatoryGridName");
			return;
		}
		std::string uri = childGetValue("login_uri_editor").asString();
		LLStringUtil::trim(uri);
		if (uri.empty())
		{
			gNotifications.add("MandatoryLoginUri");
			return;
		}
		sGridsList["grids"][i]["label"] = mGridCustomName = mGridName =
			mGridNick = name;
		sGridsList["grids"][i]["login_uri"] = mLoginURI =
			mEnteredLoginURI = uri;

		mHelperURI = childGetValue("helper_uri_editor").asString();
		LLStringUtil::trim(mHelperURI);
		sGridsList["grids"][i]["helper_uri"] = mHelperURI;

		mLoginPage = childGetValue("login_page_editor").asString();
		LLStringUtil::trim(mLoginPage);
		sGridsList["grids"][i]["login_page"] = mLoginPage;

		mWebsiteURL = childGetValue("website_editor").asString();
		LLStringUtil::trim(mWebsiteURL);
		sGridsList["grids"][i]["website_url"] = mWebsiteURL;

		mSupportURL = childGetValue("support_editor").asString();
		LLStringUtil::trim(mSupportURL);
		sGridsList["grids"][i]["support_url"] = mSupportURL;

		mAccountURL = childGetValue("new_account_editor").asString();
		LLStringUtil::trim(mAccountURL);
		sGridsList["grids"][i]["register_url"] = mAccountURL;

		mPasswordURL = childGetValue("forgotten_password_editor").asString();
		LLStringUtil::trim(mPasswordURL);
		sGridsList["grids"][i]["password_url"] = mPasswordURL;

		sGridsList["grids"][i]["can_edit"] = "true";

		mIsDirty = mIsDirtyList = mListChanged = true;
		mIsDirtyGrid = mGridNeedsUpdate = false;
	}
}

void HBPanelGridsImpl::deleteGrid()
{
	mGridDomain = mGridsScrollList->getValue().asString();
	if (mGridDomain.empty()) return;
	if (mGridDomain.find("@@|") == 0)	// Should never happen
	{
		mGridDomain = mGridDomain.substr(3);
		return;
	}

	// First, check to see if we have that grid listed in the original
	// grids list
	LLGridManager* gm = LLGridManager::getInstance();
	LLSD grids;
	gm->loadGridsLLSD(grids,
					  gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
													 "grids.xml"));
	EGridInfo i = gm->gridIndexInList(grids, mGridDomain);
	if (i == -1)
	{
		// No such grid: just delete it
		i = gm->gridIndexInList(sGridsList, mGridDomain);
		sGridsList["grids"].erase(i);
		mGridDomain.clear();
	}
	else
	{
		// Copy back the grid parameters
		EGridInfo j = gm->gridIndexInList(sGridsList, mGridDomain);
		if (j != -1)
		{
			sGridsList["grids"][j]["label"]	= mGridCustomName = mGridName =
				mGridNick = grids["grids"][i].get("label").asString();
			sGridsList["grids"][j]["login_uri"] = mLoginURI =
				mEnteredLoginURI = grids["grids"][i].get("login_uri").asString();
			sGridsList["grids"][j]["helper_uri"] = mHelperURI =
				grids["grids"][i].get("helper_uri").asString();
			sGridsList["grids"][j]["login_page"] = mLoginPage =
				grids["grids"][i].get("login_page").asString();
			sGridsList["grids"][j]["website_url"] = mWebsiteURL =
				grids["grids"][i].get("website_url").asString();
			sGridsList["grids"][j]["support_url"] = mSupportURL =
				grids["grids"][i].get("support_url").asString();
			sGridsList["grids"][j]["register_url"] = mAccountURL =
				grids["grids"][i].get("register_url").asString();
			sGridsList["grids"][j]["password_url"] = mPasswordURL =
				grids["grids"][i].get("password_url").asString();
			sGridsList["grids"][j]["can_edit"] = "false";
		}
	}

	mIsDirty = mIsDirtyList = mListChanged = true;
	mIsDirtyGrid = mGridNeedsUpdate = false;
}

// Helper functions for addGrid()

bool is_ip_address(const std::string& domain)
{
	if (domain.empty())
	{
		return true; // Pretend an empty string is an IP (saves tests).
	}
	bool result = false;
	try
	{
		std::regex ipv4_format("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}");
		result = std::regex_match(domain, ipv4_format);
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}
	return result;
}

std::string sanitize(std::string str)
{
	LLStringUtil::trim(str);
	std::string temp;
	size_t len = str.size();
	for (size_t i = 0; i < len; ++i)
	{
		char c = str[i];
		if (c == '_' || c == '-' || isalnum(c))
		{
			temp += tolower(c);
		}
		else if (c == ' ')
		{
			temp += '.';
		}
	}
	return temp;
}

void grid_exists_error(const std::string& name)
{
	LLSD args;
	args["NAME"] = name;
	gNotifications.add("ExistingGridName", args);
	return;
}

void HBPanelGridsImpl::addGrid()
{
	std::string uri = childGetValue("login_uri_editor").asString();
	LLStringUtil::trim(uri);
	if (uri.empty())
	{
		gNotifications.add("MandatoryLoginUri");
		return;
	}

	std::string name = childGetValue("grid_name_editor").asString();
	LLStringUtil::trim(name);
	if (name.empty())
	{
		gNotifications.add("MandatoryGridName");
		return;
	}
	mLoginURI = mEnteredLoginURI = uri;
	mGridCustomName = mGridName = mGridNick = name;
	mHelperURI = childGetValue("helper_uri_editor").asString();
	LLStringUtil::trim(mHelperURI);
	mLoginPage = childGetValue("login_page_editor").asString();
	LLStringUtil::trim(mLoginPage);
	mWebsiteURL = childGetValue("website_editor").asString();
	LLStringUtil::trim(mWebsiteURL);
	mAccountURL = childGetValue("new_account_editor").asString();
	LLStringUtil::trim(mAccountURL);
	mSupportURL = childGetValue("support_editor").asString();
	LLStringUtil::trim(mSupportURL);
	mPasswordURL = childGetValue("forgotten_password_editor").asString();
	LLStringUtil::trim(mPasswordURL);

	// Create an unique "domain" name that will be used as the key of this
	// grid in the grids map: this name can also be used as a grid name after
	// the --grid option in the command line of the viewer.
	mGridDomain = LLGridManager::getDomain(mLoginURI);
	if (is_ip_address(mGridDomain))
	{
		mGridDomain = LLGridManager::getDomain(mHelperURI);
		if (is_ip_address(mGridDomain))
		{
			mGridDomain = LLGridManager::getDomain(mLoginPage);
			if (is_ip_address(mGridDomain))
			{
				mGridDomain = LLGridManager::getDomain(mAccountURL);
				if (is_ip_address(mGridDomain))
				{
					mGridDomain = LLGridManager::getDomain(mSupportURL);
					if (is_ip_address(mGridDomain))
					{
						mGridDomain = LLGridManager::getDomain(mPasswordURL);
						if (is_ip_address(mGridDomain))
						{
							mGridDomain = sanitize(mGridName);
							if (is_ip_address(mGridDomain))
							{
								gNotifications.add("AddGridFailure");
								return;
							}
							mGridDomain += ".net";
						}
					}
				}
			}
		}
	}
	LLStringUtil::toLower(mGridDomain);

	// Remove some meaningless common prefixes to try and get a cleaner
	// domain name
	if (mGridDomain.find("grid.") == 0 && mGridDomain.length() > 8)
	{
		mGridDomain = mGridDomain.substr(5);
	}
	else if (mGridDomain.find("login.") == 0 && mGridDomain.length() > 9)
	{
		mGridDomain = mGridDomain.substr(6);
	}
	else if (mGridDomain.find("www.") == 0 && mGridDomain.length() > 7)
	{
		mGridDomain = mGridDomain.substr(4);
	}

	// Verify that we do not add a grid that already exists.

	if (mGridDomain == "agni.lindenlab.com" ||
		mGridDomain == "aditi.lindenlab.com")
	{
		grid_exists_error(mGridDomain);
		return;
	}

	std::string lc_name = mGridName;
	LLStringUtil::toLower(lc_name);
	if (lc_name == "secondlife" || lc_name == "secondlife beta" ||
		lc_name == "other" || lc_name == "none")
	{
		grid_exists_error(name);
		return;
	}

	EGridInfo i = LLGridManager::getInstance()->gridIndexInList(sGridsList,
																mGridDomain);
	if (i != -1)
	{
		grid_exists_error(mGridDomain);
		return;
	}

	i = LLGridManager::getInstance()->gridIndexInList(sGridsList, "", name);
	if (i != -1)
	{
		grid_exists_error(name);
		return;
	}

	// All OK: we can now add it !

	LLSD entry = sGridsList.emptyMap();
	entry.insert("name", mGridDomain);
	entry.insert("label", mGridName);
	entry.insert("login_uri", mLoginURI);
	entry.insert("helper_uri", mHelperURI);
	entry.insert("login_page", mLoginPage);
	entry.insert("website_url", mWebsiteURL);
	entry.insert("register_url", mAccountURL);
	entry.insert("support_url", mSupportURL);
	entry.insert("password_url", mPasswordURL);
	entry.insert("can_edit", "true");
	sGridsList["grids"].append(entry);

	mIsDirty = mIsDirtyList = mListChanged = true;
	mIsDirtyGrid = mGridNeedsUpdate = false;
}

void HBPanelGridsImpl::apply()
{
	if (mGridNeedsUpdate)
	{
		saveParams();
	}

	// Create a custom grids list out of listed editable grids
	LLSD grids;
	for (LLSD::map_const_iterator it = sGridsList.beginMap(),
								  end = sGridsList.endMap();
		 it != end; ++it)
	{
		LLSD::String key_name = it->first;
		LLSD grid_array = it->second;
		if (key_name == "grids" && grid_array.isArray())
		{
			for (size_t i = 0; i < grid_array.size(); ++i)
			{
				LLSD gmap = grid_array[i];
				if (gmap.has("can_edit") &&
					gmap["can_edit"].asString() == "true")
				{
					grids["grids"].append(gmap);
				}
			}
		}
	}

	// Save the custom grids list
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
														  "grids_custom.xml");
	llofstream list_file(filename.c_str());
	if (!list_file.is_open())
	{
		llwarns << "Could not open file '" << filename << "' for writing."
				<< llendl;
		return;
	}

	LLSDSerialize::toPrettyXML(grids, list_file);
	list_file.close();
	llinfos << "Saved file: " << filename << llendl;

	if (mListChanged && !LLStartUp::isLoggedIn())
	{
		LLGridManager::getInstance()->loadGridsList();
		LLStartUp::refreshLoginPanel();
	}

	// All changes saved
	mSavedGridsList.clear();
	mSavedGridsList = sGridsList;
	mListChanged = false;
}

void HBPanelGridsImpl::cancel()
{
	// Beware: cancel() is *also* called after apply() when pressing "OK" to
	//         close the Preferences floater.
	sGridsList.clear();
	sGridsList = mSavedGridsList;
	mIsDirty = mIsDirtyList = true;
}

//static
void HBPanelGridsImpl::onClickGetParams(void *data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->getParams();
	}
}

//static
void HBPanelGridsImpl::onClickClearParams(void *data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->clearParams();
	}
}

// static
void HBPanelGridsImpl::onEditorKeystroke(LLLineEditor*, void* data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->setDirty();
	}
}

//static
void HBPanelGridsImpl::onCommitCheckBoxLoginURI(LLUICtrl* ctrl, void* data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (!self || !ctrl) return;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	self->childSetValue("login_uri_editor", check->get() ? self->mLoginURI
														 : self->mEnteredLoginURI);
	self->mIsDirtyGrid = true;
}

// static
void HBPanelGridsImpl::onNameEditorKeystroke(LLLineEditor* caller, void* data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self && caller)
	{
		self->setDirty();
		S32 choice = self->childGetValue("prefer_nickname_radio").asInteger();
		if (choice != 0)
		{
			self->getChild<LLRadioGroup>("prefer_nickname_radio")->selectFirstItem();
		}
		self->mGridCustomName = caller->getValue().asString();
	}
}

//static
void HBPanelGridsImpl::onCommitRadioPreferredName(LLUICtrl* ctrl, void* data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	LLRadioGroup* radio = (LLRadioGroup*)ctrl;
	if (self && radio)
	{
		S32 choice = radio->getValue().asInteger();
		std::string name;
		switch (choice)
		{
			case 1:
				name = self->mGridName;
				break;
			case 2:
				name = self->mGridNick;
				break;
			default:
				name = self->mGridCustomName;
		}
		self->childSetValue("grid_name_editor", name);
	}
}

// static
void HBPanelGridsImpl::onSelectGrid(LLUICtrl* ctrl, void* data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->copyParams();
	}
}

//static
void HBPanelGridsImpl::onClickUpdateGrid(void *data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->saveParams();
	}
}

//static
void HBPanelGridsImpl::onClickDeleteGrid(void *data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->deleteGrid();
	}
}

//static
void HBPanelGridsImpl::onClickAddGrid(void *data)
{
	HBPanelGridsImpl* self = (HBPanelGridsImpl*)data;
	if (self)
	{
		self->addGrid();
	}
}

//---------------------------------------------------------------------------

HBPanelGrids::HBPanelGrids()
:	impl(* new HBPanelGridsImpl())
{
}

HBPanelGrids::~HBPanelGrids()
{
	delete &impl;
}

void HBPanelGrids::apply()
{
	impl.apply();
}

void HBPanelGrids::cancel()
{
	impl.cancel();
}

LLPanel* HBPanelGrids::getPanel()
{
	return &impl;
}
