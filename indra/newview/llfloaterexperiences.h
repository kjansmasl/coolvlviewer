/**
 * @file llfloaterexperiences.h
 * @brief LLFloaterExperiences class definition
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLFLOATEREXPERIENCES_H
#define LL_LLFLOATEREXPERIENCES_H

#include <map>

#include "llcorehttputil.h"
#include "llfloater.h"

class LLButton;
class LLScrollListCtrl;
class LLTabContainer;

class LLPanelExperiences : public LLPanel
{
public:
	LLPanelExperiences();

	static LLPanelExperiences* create(const std::string& name);

	bool postBuild() override;

	void setExperienceList(const LLSD& experiences);

	void addExperience(const LLUUID& id);
	void removeExperience(const LLUUID& id);
	void removeExperiences(const LLSD& ids);

	typedef void (*click_callback)(void*);
	void setButtonAction(const std::string& label, void (*cb)(void*),
						 void* data = NULL);
	void enableButton(bool enable);

#if 0	// not used
	std::string	getSelectedExperienceName() const;
	LLUUID		getSelectedExperienceId() const;
#endif

private:
	static void cacheCallback(LLHandle<LLPanelExperiences> handle,
							  const LLSD& experience);

	static void onDoubleClickProfile(void* data);

private:
	LLButton*			mActionBtn;
	LLScrollListCtrl*	mExperiencesList;

	bool				mListEmpty;
};

class LLFloaterExperiences final : public LLFloater,
								   public LLFloaterSingleton<LLFloaterExperiences>
{
	friend class LLUISingleton<LLFloaterExperiences,
							   VisibilityPolicy<LLFloater> >;
	friend class LLExperienceListResponder;

protected:
	LOG_CLASS(LLFloaterExperiences);

public:
	~LLFloaterExperiences() override;

	bool postBuild() override;
    void onClose(bool app_quitting) override;
    void onOpen() override;

	static void show();

protected:
	LLFloaterExperiences(const LLSD&);

    void refreshContents();

    LLPanelExperiences* addTab(const std::string& name, bool select);

    bool updatePermissions(const LLSD& permission);

	void checkPurchaseInfo(LLPanelExperiences* panel, const LLSD& content);
	void updateInfo(const char* experiences, const char* tab);

	void doSendPurchaseRequest();

	static void sendPurchaseRequest(void* data);
	static void onCloseBtn(void* data);

	typedef std::map<std::string, std::string> name_map_t;
	typedef boost::function<void(LLPanelExperiences*, const LLSD&)> Callback_t;
	void retrieveExperienceList(const std::string& url,
								const LLHandle<LLFloaterExperiences>& handle,
								const name_map_t& tab_map,
								const std::string& error_notify = "ErrorMessage",
								Callback_t cb = Callback_t());
	void requestNewExperience(const std::string& url,
							  const LLHandle<LLFloaterExperiences>& handle,
							  const name_map_t& tab_map,
							  const std::string& error_notify,
							  Callback_t cb);

private:
	typedef boost::function<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t,
								 const std::string, LLCore::HttpOptions::ptr_t,
								 LLCore::HttpHeaders::ptr_t)> invokationFn_t;
	static void retrieveExperienceListCoro(std::string url, 
										   LLHandle<LLFloaterExperiences> handle,
										   name_map_t tab_map, 
										   std::string error_notify,
										   Callback_t cb,
										   invokationFn_t invoker);
	
private:
	LLTabContainer*					mTabContainer;

	static S32						sLastTab;
};

#endif //LL_LLFLOATEREXPERIENCES_H
