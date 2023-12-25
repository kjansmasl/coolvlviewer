/**
 * @file slfloatermediafilter.cpp
 * @brief The SLFloaterMediaFilter class definitions
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Sione Lomu
 * with debugging and improvements by Henri Beauchamp
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

#include "slfloatermediafilter.h"

#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llviewercontrol.h"
#include "llviewermedia.h"

bool SLFloaterMediaFilter::sIsWhitelist = false;
bool SLFloaterMediaFilter::sShowIPs = false;

SLFloaterMediaFilter::SLFloaterMediaFilter(const LLSD&)
:	mIsDirty(true)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_media_filter.xml");
}

//virtual
bool SLFloaterMediaFilter::postBuild()
{
	mWhitelistSLC = getChild<LLScrollListCtrl>("whitelist_list");
	mBlacklistSLC = getChild<LLScrollListCtrl>("blacklist_list");

	childSetAction("clear_lists", onClearLists, this);
	childSetAction("show_ips", onShowIPs, this);
	childSetAction("add_whitelist", onWhitelistAdd, this);
	childSetAction("remove_whitelist", onWhitelistRemove, this);
	childSetAction("add_blacklist", onBlacklistAdd, this);
	childSetAction("remove_blacklist", onBlacklistRemove, this);
	childSetAction("commit_domain", onCommitDomain, this);
	childSetUserData("whitelist_list", this);
	childSetUserData("blacklist_list", this);

	return true;
}

//virtual
void SLFloaterMediaFilter::draw()
{
	LLTimer update_timer;

	if (mIsDirty)
	{
		S32 whitescrollpos = mWhitelistSLC->getScrollPos();
		S32 blackscrollpos = mBlacklistSLC->getScrollPos();
		mWhitelistSLC->deleteAllItems();
		mBlacklistSLC->deleteAllItems();
		std::set<std::string> listed;
		std::string ip;
		std::string domain;
		std::string action;
		LLSD element;
		element["columns"][0]["font"] = "SANSSERIF";
		element["columns"][0]["font-style"] = "BOLD";
		for (S32 i = 0; i < (S32)LLViewerMedia::sMediaFilterList.size(); ++i)
		{
			domain = LLViewerMedia::sMediaFilterList[i]["domain"].asString();
			if (sShowIPs && update_timer.getElapsedTimeF32() < 1.0f)
			{
				ip = LLViewerMedia::getDomainIP(domain, true);
				if (ip != domain && domain.find('/') == std::string::npos)
				{
					domain += " (" + ip + ")";
				}
			}

			action = LLViewerMedia::sMediaFilterList[i]["action"].asString();
			if (!domain.empty() && action == "allow")
			{
				element["columns"][0]["column"] = "whitelist_col";
				element["columns"][0]["value"] = domain;
				//element["columns"][0]["color"] = LLColor4::green3.getValue();
				mWhitelistSLC->addElement(element, ADD_BOTTOM);
				listed.emplace(domain);
			}
			else if (!domain.empty() && action == "deny")
			{
				element["columns"][0]["column"] = "blacklist_col";
				element["columns"][0]["value"] = domain;
				//element["columns"][0]["color"] = LLColor4::red2.getValue();
				mBlacklistSLC->addElement(element, ADD_BOTTOM);
				listed.emplace(domain);
			}
			else
			{
				llwarns << "Bad media filter list: removing corrupted entry for \""
						<< domain << "\"" << llendl;
				LLViewerMedia::sMediaFilterList.erase(i--);
			}
		}
		element["columns"][0]["font"] = "SANSSERIF";
		element["columns"][0]["font-style"] = "ITALIC";
#if 0
		element["columns"][0]["color"] = LLColor4::green3.getValue();
#endif
		element["columns"][0]["column"] = "whitelist_col";
		for (std::set<std::string>::iterator
				it = LLViewerMedia::sAllowedMedia.begin(),
				end = LLViewerMedia::sAllowedMedia.end();
			 it != end; ++it)
		{
			domain = *it;
			if (sShowIPs && update_timer.getElapsedTimeF32() < 1.0f)
			{
				ip = LLViewerMedia::getDomainIP(domain, true);
				if (ip != domain && domain.find('/') == std::string::npos)
				{
					domain += " (" + ip + ")";
				}
			}
			if (listed.count(domain) == 0)
			{
				element["columns"][0]["value"] = domain;
				mWhitelistSLC->addElement(element, ADD_BOTTOM);
			}
		}
		element["columns"][0]["column"] = "blacklist_col";
		for (std::set<std::string>::iterator
				it = LLViewerMedia::sDeniedMedia.begin(),
				end = LLViewerMedia::sDeniedMedia.end();
			 it != end; ++it)
		{
			domain = *it;
			if (sShowIPs && update_timer.getElapsedTimeF32() < 1.0f)
			{
				ip = LLViewerMedia::getDomainIP(domain, true);
				if (ip != domain && domain.find('/') == std::string::npos)
				{
					domain += " (" + ip + ")";
				}
			}
			if (listed.count(domain) == 0)
			{
				element["columns"][0]["value"] = domain;
				mBlacklistSLC->addElement(element, ADD_BOTTOM);
			}
		}
		mWhitelistSLC->setScrollPos(whitescrollpos);
		mBlacklistSLC->setScrollPos(blackscrollpos);

		if (!gSavedSettings.getBool("MediaEnableFilter"))
		{
			childDisable("clear_lists");
			childDisable("show_ips");
			childDisable("blacklist_list");
			childDisable("whitelist_list");
			childDisable("remove_whitelist");
			childDisable("add_whitelist");
			childDisable("remove_blacklist");
			childDisable("add_blacklist");
			childDisable("match_ip");
			childDisable("input_domain");
			childDisable("commit_domain");
			childSetText("add_text", getString("disabled"));
		}

		if (sShowIPs)
		{
			if (update_timer.getElapsedTimeF32() < 1.0f)
			{
				mIsDirty = sShowIPs = false;
			}
		}
		else
		{
			mIsDirty = false;
		}
	}

	LLFloater::draw();
}

void SLFloaterMediaFilter::setDirty()
{
	SLFloaterMediaFilter* self = findInstance();
	if (self)
	{
		self->mIsDirty = true;
	}
}

void SLFloaterMediaFilter::onClearLists(void*)
{
	LLViewerMedia::clearDomainFilterList();
}

void SLFloaterMediaFilter::onShowIPs(void*)
{
	sShowIPs = true;
	setDirty();
}

void SLFloaterMediaFilter::onWhitelistAdd(void* data)
{
	SLFloaterMediaFilter* self = (SLFloaterMediaFilter*)data;
	if (self)
	{
		self->childDisable("clear_lists");
		self->childDisable("show_ips");
		self->childDisable("blacklist_list");
		self->childDisable("whitelist_list");
		self->childDisable("remove_whitelist");
		self->childDisable("add_whitelist");
		self->childDisable("remove_blacklist");
		self->childDisable("add_blacklist");
		self->childEnable("input_domain");
		self->childEnable("commit_domain");
		self->childSetText("add_text", self->getString("white_prompt"));
		sIsWhitelist = true;
	}
}

void SLFloaterMediaFilter::onWhitelistRemove(void* data)
{
	SLFloaterMediaFilter* self = (SLFloaterMediaFilter*)data;
	if (!self)
	{
		return;
	}
	LLScrollListItem* selected = self->mWhitelistSLC->getFirstSelected();
	if (selected)
	{
		std::string domain = self->mWhitelistSLC->getSelectedItemLabel();
		size_t pos = domain.find(' ');
		if (pos != std::string::npos)
		{
			domain = domain.substr(0, pos);
		}

		LLViewerMedia::sAllowedMedia.erase(domain);

		for (S32 i = 0, count = LLViewerMedia::sMediaFilterList.size();
			 i < count; ++i)
		{
			if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerMedia::sMediaFilterList.erase(i);
				break;
			}
		}

		if (self->childGetValue("match_ip") &&
			domain.find('/') == std::string::npos)
		{
			std::string ip = LLViewerMedia::getDomainIP(domain, true);

			if (ip != domain)
			{
				LLViewerMedia::sAllowedMedia.erase(ip);

				for (S32 i = 0, count = LLViewerMedia::sMediaFilterList.size();
					 i < count; ++i)
				{
					if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == ip)
					{
						LLViewerMedia::sMediaFilterList.erase(i);
						break;
					}
				}
			}
		}

		LLViewerMedia::saveDomainFilterList();
		setDirty();
	}
}

void SLFloaterMediaFilter::onBlacklistAdd(void* data)
{
	SLFloaterMediaFilter* self = (SLFloaterMediaFilter*)data;
	if (self)
	{
		self->childDisable("clear_lists");
		self->childDisable("show_ips");
		self->childDisable("blacklist_list");
		self->childDisable("whitelist_list");
		self->childDisable("remove_whitelist");
		self->childDisable("add_whitelist");
		self->childDisable("remove_blacklist");
		self->childDisable("add_blacklist");
		self->childEnable("input_domain");
		self->childEnable("commit_domain");
		self->childSetText("add_text", self->getString("black_prompt"));
		sIsWhitelist = false;
	}
}

void SLFloaterMediaFilter::onBlacklistRemove(void* data)
{
	SLFloaterMediaFilter* self = (SLFloaterMediaFilter*)data;
	if (!self)
	{
		return;
	}

	LLScrollListItem* selected = self->mBlacklistSLC->getFirstSelected();
	if (selected)
	{
		std::string domain = self->mBlacklistSLC->getSelectedItemLabel();
		size_t pos = domain.find(' ');
		if (pos != std::string::npos)
		{
			domain = domain.substr(0, pos);
		}

		LLViewerMedia::sDeniedMedia.erase(domain);

		for (S32 i = 0, count = LLViewerMedia::sMediaFilterList.size();
			 i < count; ++i)
		{
			if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerMedia::sMediaFilterList.erase(i);
				break;
			}
		}

		if (self->childGetValue("match_ip") &&
			domain.find('/') == std::string::npos)
		{
			std::string ip = LLViewerMedia::getDomainIP(domain, true);

			if (ip != domain)
			{
				LLViewerMedia::sDeniedMedia.erase(ip);

				for (S32 i = 0, count = LLViewerMedia::sMediaFilterList.size();
					 i < count; ++i)
				{
					if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == ip)
					{
						LLViewerMedia::sMediaFilterList.erase(i);
						break;
					}
				}
			}
		}

		LLViewerMedia::saveDomainFilterList();
		setDirty();
	}
}

void SLFloaterMediaFilter::onCommitDomain(void* data)
{
	SLFloaterMediaFilter* self = (SLFloaterMediaFilter*)data;
	if (!self)
	{
		return;
	}

	std::string domain = self->childGetText("input_domain");
	domain = LLViewerMedia::extractDomain(domain);
	std::string ip = domain;
	bool match_ip = (self->childGetValue("match_ip") &&
					 domain.find('/') == std::string::npos);
	if (match_ip)
	{
		ip = LLViewerMedia::getDomainIP(domain, true);
		match_ip = (ip != domain);
	}

	if (!domain.empty())
	{
		LLViewerMedia::sDeniedMedia.erase(domain);
		LLViewerMedia::sAllowedMedia.erase(domain);
		for (S32 i = 0; i < (S32)LLViewerMedia::sMediaFilterList.size(); )
		{
			if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerMedia::sMediaFilterList.erase(i);
			}
			else
			{
				++i;
			}
		}
		if (match_ip)
		{
			LLViewerMedia::sDeniedMedia.erase(ip);
			LLViewerMedia::sAllowedMedia.erase(ip);
			for (S32 i = 0; i < (S32)LLViewerMedia::sMediaFilterList.size(); )
			{
				if (LLViewerMedia::sMediaFilterList[i]["domain"].asString() == ip)
				{
					LLViewerMedia::sMediaFilterList.erase(i);
				}
				else
				{
					++i;
				}
			}
		}
		LLSD newmedia;
		newmedia["domain"] = domain;
		if (sIsWhitelist)
		{
			newmedia["action"] = "allow";
		}
		else
		{
			newmedia["action"] = "deny";
		}
		LLViewerMedia::sMediaFilterList.append(newmedia);
		if (match_ip)
		{
			newmedia["domain"] = ip;
			LLViewerMedia::sMediaFilterList.append(newmedia);
		}
		LLViewerMedia::saveDomainFilterList();
	}

	self->childEnable("clear_lists");
	self->childEnable("show_ips");
	self->childEnable("blacklist_list");
	self->childEnable("whitelist_list");
	self->childEnable("remove_whitelist");
	self->childEnable("add_whitelist");
	self->childEnable("remove_blacklist");
	self->childEnable("add_blacklist");
	self->childDisable("input_domain");
	self->childDisable("commit_domain");
	self->childSetText("add_text", self->getString("domain_prompt"));
	self->childSetText("input_domain", "");
	setDirty();
}
