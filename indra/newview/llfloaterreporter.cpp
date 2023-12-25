/**
 * @file llfloaterreporter.cpp
 * @brief Bug and abuse reports.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include <sstream>

#include "llfloaterreporter.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcombobox.h"
#include "llexperiencecache.h"
#include "llfilesystem.h"
#include "llimagej2c.h"
#include "llinventory.h"
#include "lllineeditor.h"
#include "llsys.h"
#include "lltexteditor.h"
#include "lltransfermanager.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"
#include "llversionviewer.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloateravatarpicker.h"
#include "llgridmanager.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "lltoolmgr.h"
#include "lltoolobjpicker.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

// Specialized upload ressource info
class LLARScreenShotUploader final : public LLResourceUploadInfo
{
public:
	LLARScreenShotUploader(const LLSD& report, const LLUUID& asset_id,
						   LLAssetType::EType type)
	:	LLResourceUploadInfo(asset_id, type, "Abuse Report"),
		mReport(report)
	{
	}

    LLSD prepareUpload() override
	{
		return LLSD().with("success", LLSD::Boolean(true));
	}

    LLSD generatePostBody() override				{ return mReport; }

    S32 getExpectedUploadCost() override			{ return 0; }

    LLUUID finishUpload(const LLSD&) override		{ return LLUUID::null; }

    bool showInventoryPanel() const 				{ return false; }

    std::string getDisplayName() const override		{ return "Abuse Report"; }

private:
	LLSD mReport;
};

LLFloaterReporter::LLFloaterReporter(const LLSD&)
:	mPicking(false),
	mCopyrightWarningSeen(false),
	mResourceDatap(new LLResourceData())
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_report_abuse.xml");
}

//virtual
LLFloaterReporter::~LLFloaterReporter()
{
	// Child views automatically deleted
	mObjectID.setNull();

	if (mPicking)
	{
		closePickTool(this);
	}

	mPosition.set(0.0f, 0.0f, 0.0f);

	delete mResourceDatap;
}

//virtual
bool LLFloaterReporter::postBuild()
{
	childSetText("abuse_location_edit", gAgent.getSLURL());
	LLButton* pick_btn = getChild<LLButton>("pick_btn");
	pick_btn->setImages("UIImgFaceUUID", "UIImgFaceSelectedUUID");
	pick_btn->setClickedCallback(onClickObjPicker, this);

	// Abuser name is selected from a list
	LLLineEditor* le = getChild<LLLineEditor>("abuser_name_edit");
	le->setEnabled(false);

	childSetAction("select_abuser", onClickSelectAbuser, this);

	childSetAction("send_btn", onClickSend, this);
	childSetAction("cancel_btn", onClickCancel, this);

	// Convert the position to a string
	LLVector3d pos = gAgent.getPositionGlobal();
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		pos -= regionp->getOriginGlobal();
	}
	setPosBox(pos);

	// Take a screenshot, but do not draw this floater.
	setVisible(false);
	takeScreenshot();
	setVisible(true);

	// Default text to be blank
	childSetText("object_name", LLStringUtil::null);
	childSetText("owner_name", LLStringUtil::null);
	mOwnerName = LLStringUtil::null;

	childSetFocus("summary_edit");

	mDefaultSummary = childGetText("details_edit");

	mCategoryCombo = getChild<LLComboBox>("category_combo");

	std::string cap_url = gAgent.getRegionCapability("AbuseCategories");
	if (!cap_url.empty())
	{
		std::string lang = gSavedSettings.getString("Language");
		if (!lang.empty())
		{
			cap_url += "?lc=" + lang;
		}
		gCoros.launch("requestAbuseCategoriesCoro",
					  boost::bind(&LLFloaterReporter::requestAbuseCategoriesCoro,
								  cap_url));
	}

	center();

	return true;
}

//static
void LLFloaterReporter::requestAbuseCategoriesCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("requestAbuseCategoriesCoro");
	LLSD result = adapter.getAndSuspend(url);

	LLFloaterReporter* self = findInstance();
	if (!self)
	{
		return; // Floater has since been closed !
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("categories"))
	{
		llwarns << "Error requesting abuse categories from capability. Error: "
				<< status.toString() << llendl;
		return;
	}

	const LLSD& contents = result["categories"];
	if (!contents.size())
	{
		llwarns << "No contents received for abuse categories" << llendl;
		return;
	}

	llinfos << "Populating abuse report categories combo from server data"
			<< llendl;

	LLComboBox* combo = self->mCategoryCombo;
	// Remember selection
	S32 selection = combo->getCurrentIndex();

	// Get the first item ("Select a category" entry) label and value
	combo->selectFirstItem();
	std::string label = combo->getSelectedItemLabel();
	LLSD value = combo->getSelectedValue();
	// Clear the whole combo
	combo->removeall();
	// Re-add the first entry
	combo->add(label, value);

	// Add the recceived categories
	for (LLSD::array_const_iterator it = contents.beginArray(),
									end = contents.endArray();
		 it != end; ++it)
	{
		const LLSD& message_data = *it;
		label = message_data["description_localized"].asString();
		combo->add(label, message_data["category"]);
	}

	// Restore selection
	combo->selectNthItem(selection);
}

void LLFloaterReporter::getObjectInfo(const LLUUID& object_id)
{
	// *TODO:
	// 1.- need to send to correct simulator if object is not in same simulator
	//     as agent
	// 2.- display info in widget window that gives feedback that we have
	//     recorded the object info
	// 3.- can pick avatar ==> might want to indicate when a picked object is
	//     an avatar, attachment, or other category

	mObjectID = object_id;

	if (mObjectID.notNull())
	{
		// Get object info for the user's benefit
		LLViewerObject* objectp = gObjectList.findObject(mObjectID);
		if (objectp)
		{
			// Use the root object (for attachments, it will also pick the
			// avatar wearing it, which is what we want). This is important,
			// since passing a child object info request to the simulator would
			// fail to get a reply sent back to the reporter (server bug ?...
			// COMPLAINT_REPORT_REQUEST flag lost ?).
			objectp = (LLViewerObject*)objectp->getRoot();
			mObjectID = objectp->getID();

			// Correct the region and position information
			LLViewerRegion* regionp = objectp->getRegion();
			if (regionp)
			{
				childSetText("sim_field", regionp->getName());
				LLVector3d global_pos;
				global_pos.set(objectp->getPositionRegion());
				setPosBox(global_pos);
			}

			if (objectp->isAvatar())
			{
				setFromAvatarID(mObjectID);
			}
			else if (regionp)
			{
				// We have to query the simulator for information about this
				// object
				LLSelectMgr::registerObjectPropertiesFamilyRequest(mObjectID);
				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags,
								COMPLAINT_REPORT_REQUEST);
				msg->addUUIDFast(_PREHASH_ObjectID, mObjectID);
				msg->sendReliable(regionp->getHost());
			}
			else
			{
				llwarns << "NULL region pointer for object: " << mObjectID
						<< ". Cannot request info..." << llendl;
			}
		}
	}
}

void LLFloaterReporter::getExperienceInfo(const LLUUID& experience_id)
{
	mExperienceID = experience_id;

	if (mExperienceID.notNull())
	{
		std::stringstream desc;
		const LLSD& experience =
			LLExperienceCache::getInstance()->get(mExperienceID);
		if (experience.isDefined())
		{
			setFromAvatarID(experience[LLExperienceCache::AGENT_ID]);
			desc << "Experience id: " << mExperienceID;
		}
		else
		{
			desc << "Unable to retrieve details for id: "<< mExperienceID;
		}

		childSetText("details_edit", desc.str());
	}
}

//static
void LLFloaterReporter::onClickSelectAbuser(void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;
	if (!self || !gFloaterViewp)
	{
		return;
	}
	LLFloater* parent = gFloaterViewp->getParentFloater(self);
	if (!parent)
	{
		return;
	}
	parent->addDependentFloater(LLFloaterAvatarPicker::show(callbackAvatarID,
															userdata,
															false, true));
}

//static
void LLFloaterReporter::callbackAvatarID(const std::vector<std::string>& names,
										 const std::vector<LLUUID>& ids,
										 void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;
	if (self && !ids.empty() && !names.empty())
	{
		self->childSetText("abuser_name_edit", names[0]);
		self->mAbuserID = ids[0];
		self->refresh();
	}
}

//static
void LLFloaterReporter::onClickSend(void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;
	if (!self) return;

	if (self->mPicking)
	{
		closePickTool(self);
	}

	if (self->validateReport())
	{
		constexpr int IP_CONTENT_REMOVAL = 66;
		constexpr int IP_PERMISSONS_EXPLOIT = 37;
		LLComboBox* combo = self->mCategoryCombo;
		S32 category_value = combo->getSelectedValue().asInteger();

		if (!self->mCopyrightWarningSeen)
		{
			std::string details_lc = self->childGetText("details_edit");
			LLStringUtil::toLower(details_lc);
			std::string summary_lc = self->childGetText("summary_edit");
			LLStringUtil::toLower(summary_lc);
			if (details_lc.find("copyright") != std::string::npos ||
				summary_lc.find("copyright") != std::string::npos  ||
				category_value == IP_CONTENT_REMOVAL ||
				category_value == IP_PERMISSONS_EXPLOIT)
			{
				gNotifications.add("HelpReportAbuseContainsCopyright");
				self->mCopyrightWarningSeen = true;
				return;
			}
		}
		else if (category_value == IP_CONTENT_REMOVAL)
		{
			// IP_CONTENT_REMOVAL *always* shows the dialog -
			// ergo you can never send that abuse report type.
			gNotifications.add("HelpReportAbuseContainsCopyright");
			return;
		}

		LLUploadDialog::modalUploadDialog("Uploading...\n\nReport");
		// *TODO don't upload image if checkbox isn't checked
		const std::string& url = gAgent.getRegionCapability("SendUserReport");
		const std::string& sshot_url =
			gAgent.getRegionCapability("SendUserReportWithScreenshot");
		if (!url.empty() || !sshot_url.empty())
		{
			self->sendReportViaCaps(url, sshot_url, self->gatherReport());
			self->close();
		}
		else if (self->childGetValue("screen_check"))
		{
			self->childDisable("send_btn");
			self->childDisable("cancel_btn");
			// The callback from uploading the image calls
			// sendReportViaLegacy()
			self->uploadImage();
		}
		else
		{
			self->sendReportViaLegacy(self->gatherReport());
			LLUploadDialog::modalUploadFinished();
			self->close();
		}
	}
}

//static
void LLFloaterReporter::onClickCancel(void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;

	// Reset flag in case the next report also contains this text
	self->mCopyrightWarningSeen = false;

	if (self->mPicking)
	{
		closePickTool(self);
	}
	self->close();
}

//static
void LLFloaterReporter::onClickObjPicker(void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;
	if (!self) return;

	gToolObjPicker.setExitCallback(LLFloaterReporter::closePickTool, self);
	gToolMgr.setTransientTool(&gToolObjPicker);
	self->mPicking = true;
	self->childSetText("object_name", LLStringUtil::null);
	self->childSetText("owner_name", LLStringUtil::null);
	self->mOwnerName = LLStringUtil::null;
	self->getChild<LLButton>("pick_btn")->setToggleState(true);
}

//static
void LLFloaterReporter::closePickTool(void* userdata)
{
	LLFloaterReporter* self = (LLFloaterReporter*)userdata;
	if (!self) return;

	const LLUUID& object_id = gToolObjPicker.getObjectID();
	self->getObjectInfo(object_id);

	gToolMgr.clearTransientTool();
	self->mPicking = false;
	self->getChild<LLButton>("pick_btn")->setToggleState(false);
}

//static
bool LLFloaterReporter::showFromMenu()
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShowloc ||
		 gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		gNotifications.add("RLVCannotAbuseReport");
		return false;
	}
//mk
	LLFloaterReporter* self = findInstance();
	if (self)
	{
		// Bring that window to front
		self->open();
	}
	else
	{
		createNewReporter();
		LLFloaterReporter* self = findInstance();
		std::string fullname;
		gAgent.buildFullname(fullname);
		self->childSetText("reporter_field", fullname);
	}
	return true;
}

//static
void LLFloaterReporter::showFromAvatar(const LLUUID& avatar_id,
									   const std::string& desc,
									   S32 abuse_category)
{
	if (avatar_id.isNull() || !showFromMenu())
	{
		return;
	}
	LLFloaterReporter* self = findInstance();
	self->setFromAvatarID(avatar_id);
	if (!desc.empty())
	{
		self->childSetText("details_edit", desc);
	}
	if (abuse_category > -1)
	{
		self->mCategoryCombo->setSelectedByValue(LLSD::Integer(abuse_category),
												 true);
	}
}

//static
void LLFloaterReporter::showFromObject(const LLUUID& object_id,
									   const LLUUID& experience_id)
{
	if (!showFromMenu())
	{
		return;
	}

	LLFloaterReporter* self = findInstance();

	if (gObjectList.findAvatar(object_id))
	{
		self->setFromAvatarID(object_id);
	}
	else
	{
		// Request info for this object
		self->getObjectInfo(object_id);
	}

	self->getExperienceInfo(experience_id);
}

//static
void LLFloaterReporter::showFromExperience(const LLUUID& experience_id)
{
	if (showFromMenu())
	{
		LLFloaterReporter* self = findInstance(); // Guaranteed to exist
		self->getExperienceInfo(experience_id);
	}
}

//static
void LLFloaterReporter::onAvatarNameCache(const LLUUID& avatar_id,
										  const LLAvatarName& av_name)
{
	LLFloaterReporter* self = findInstance();
	if (!self) return;	// Stale callback, floater closed...

	self->mOwnerName = av_name.getNames();
	self->childSetText("owner_name", self->mOwnerName);
	self->childSetText("abuser_name_edit", self->mOwnerName);

	if (self->mObjectID == avatar_id)
	{
		self->childSetText("object_name", self->mOwnerName);
	}
}

void LLFloaterReporter::setFromAvatarID(const LLUUID& avatar_id)
{
	mAbuserID = mObjectID = avatar_id;
	LLAvatarNameCache::get(avatar_id,
						   boost::bind(&LLFloaterReporter::onAvatarNameCache,
									   _1, _2));
}

LLFloaterReporter* LLFloaterReporter::createNewReporter()
{
	LLFloaterReporter* self = findInstance();
	if (self)
	{
		// Only one reporter allowed at any time !
		self->close();
	}
	return getInstance();	// Creates a new reporter
}

void LLFloaterReporter::setPickedObjectProperties(const std::string& object_name,
												  const std::string& owner_name,
												  const LLUUID& owner_id)
{
	childSetText("object_name", object_name);
	childSetText("owner_name", owner_name);
	childSetText("abuser_name_edit", owner_name);
	mAbuserID = owner_id;
	mOwnerName = owner_name;
	if (mOwnerName.empty())
	{
		LLAvatarNameCache::get(owner_id,
							   boost::bind(&LLFloaterReporter::onAvatarNameCache,
										   _1, _2));
	}
}

bool LLFloaterReporter::validateReport()
{
	// Ensure user selected a category from the list
	LLSD category_sd = mCategoryCombo->getValue();
	U8 category = (U8)category_sd.asInteger();
	if (category == 0)
	{
		gNotifications.add("HelpReportAbuseSelectCategory");
		return false;
	}

	if (childGetText("abuser_name_edit").empty())
	{
		gNotifications.add("HelpReportAbuseAbuserNameEmpty");
		return false;
	}

	if (childGetText("abuse_location_edit").empty())
	{
		gNotifications.add("HelpReportAbuseAbuserLocationEmpty");
		return false;
	}

	if (childGetText("summary_edit").empty())
	{
		gNotifications.add("HelpReportAbuseSummaryEmpty");
		return false;
	}

	if (childGetText("details_edit") == mDefaultSummary)
	{
		gNotifications.add("HelpReportAbuseDetailsEmpty");
		return false;
	}

	return true;
}

LLSD LLFloaterReporter::gatherReport()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		return LLSD(); // *TODO handle this failure case more gracefully
	}

	// Reset flag in case the next report also contains this text
	mCopyrightWarningSeen = false;

	std::ostringstream summary;
	if (!gIsInProductionGrid)
	{
		summary << "Preview ";
	}

	std::string category_name;
	if (mCategoryCombo)
	{
		// We want the label, not the value
		category_name = mCategoryCombo->getSelectedItemLabel();
	}

#if LL_WINDOWS
	const char* platform = "Win";
#elif LL_DARWIN
	const char* platform = "Mac";
#elif LL_LINUX
	const char* platform = "Lnx";
#else
	const char* platform = "???";
#endif

			// Region in which the reporter is currently present.
	summary << " |" << regionp->getName() << "|"
			// Region where abuse occured (freeform text)
			<< " (" << childGetText("abuse_location_edit") << ")"
			// Updated category
			<< " [" << category_name << "] "
			// Name of abuser entered in report (chosen using LLAvatarPicker)
			<< " {" << childGetText("abuser_name_edit") << "} "
			// Summary as freeform text
			<< " \"" << childGetValue("summary_edit").asString()
			<< "\"";

	std::ostringstream details;
	// Client version moved to body of email for abuse reports
	details << "V" << LL_VERSION_MAJOR << "." << LL_VERSION_MINOR << "."
			<< LL_VERSION_BRANCH << "." << LL_VERSION_RELEASE << std::endl;

	std::string object_name = childGetText("object_name");
	if (!object_name.empty() && !mOwnerName.empty())
	{
		details << "Object: " << object_name << "\n";
		details << "Owner: " << mOwnerName << "\n";
	}

	details << "Abuser name: " << childGetText("abuser_name_edit") << " \n";
	details << "Abuser location: " << childGetText("abuse_location_edit")
			<< " \n";

	details << childGetValue("details_edit").asString();

	std::string version_string;
	version_string = llformat("%d.%d.%d %s %s %s %s", LL_VERSION_MAJOR,
							  LL_VERSION_MINOR, LL_VERSION_BRANCH, platform,
							  LLCPUInfo::getInstance()->getFamily().c_str(),
							  gGLManager.mGLRenderer.c_str(),
							  gGLManager.mDriverVersionVendorString.c_str());

	LLUUID screenshot_id;
	if (childGetValue("screen_check"))
	{
		screenshot_id = childGetValue("screenshot");
	}

	LLSD report = LLSD::emptyMap();
	report["report-type"] = (U8)COMPLAINT_REPORT;
	report["category"] = mCategoryCombo->getValue();
	report["position"] = mPosition.getValue();
	report["check-flags"] = (U8)0; // this is not used
	report["screenshot-id"] = screenshot_id;
	report["object-id"] = mObjectID;
	report["abuser-id"] = mAbuserID;
	report["abuse-region-name"] = "";
	report["abuse-region-id"] = LLUUID::null;
	report["summary"] = summary.str();
	report["version-string"] = version_string;
	report["details"] = details.str();
	return report;
}

void LLFloaterReporter::sendReportViaLegacy(const LLSD& report)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_UserReport);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlockFast(_PREHASH_ReportData);
	msg->addU8Fast(_PREHASH_ReportType, report["report-type"].asInteger());
	msg->addU8(_PREHASH_Category, report["category"].asInteger());
	msg->addVector3Fast(_PREHASH_Position, LLVector3(report["position"]));
	msg->addU8Fast(_PREHASH_CheckFlags, report["check-flags"].asInteger());
	msg->addUUIDFast(_PREHASH_ScreenshotID, report["screenshot-id"].asUUID());
	msg->addUUIDFast(_PREHASH_ObjectID, report["object-id"].asUUID());
	msg->addUUID("AbuserID", report["abuser-id"].asUUID());
	msg->addString("AbuseRegionName", report["abuse-region-name"].asString());
	msg->addUUID("AbuseRegionID", report["abuse-region-id"].asUUID());

	msg->addStringFast(_PREHASH_Summary, report["summary"].asString());
	msg->addString("VersionString", report["version-string"]);
	msg->addStringFast(_PREHASH_Details, report["details"]);

	msg->sendReliable(regionp->getHost());
}

void LLFloaterReporter::finishedARPost(const LLSD&)
{
	LLUploadDialog::modalUploadFinished();
}

void LLFloaterReporter::sendReportViaCaps(const std::string& url,
										  const std::string& sshot_url,
										  const LLSD& report)
{
	if (childGetValue("screen_check").asBoolean() && !sshot_url.empty())
	{
		// Try to upload screenshot
		LLResourceUploadInfo::ptr_t
			info(new LLARScreenShotUploader(report,
											mResourceDatap->mAssetInfo.mUuid,
											mResourceDatap->mAssetInfo.mType));
		LLViewerAssetUpload::enqueueInventoryUpload(sshot_url, info);
	}
	else
	{
		// Screenshot not wanted or we do not have screenshot cap
		LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t
				proc = boost::bind(&LLFloaterReporter::finishedARPost, _1);
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, report,
															   proc, proc);
	}
}

void LLFloaterReporter::takeScreenshot()
{
	constexpr S32 IMAGE_WIDTH = 1024;
	constexpr S32 IMAGE_HEIGHT = 768;

	mImageRaw = new LLImageRaw;
	if (!gViewerWindowp->rawSnapshot(mImageRaw, IMAGE_WIDTH, IMAGE_HEIGHT,
									 true, false, true, false))
	{
		llwarns << "Unable to take screenshot" << llendl;
		return;
	}
	LLPointer<LLImageJ2C> j2cp =
		LLViewerTextureList::convertToUploadFile(mImageRaw);
	if (!j2cp)
	{
		llwarns << "Unable to encode the screenshot" << llendl;
		return;
	}

	// Create a resource data
	mResourceDatap->mInventoryType = LLInventoryType::IT_NONE;
	mResourceDatap->mNextOwnerPerm = 0;	// Not used
	// We expect that abuse screenshots are free:
	mResourceDatap->mExpectedUploadCost = 0;
	mResourceDatap->mAssetInfo.mTransactionID.generate();
	mResourceDatap->mAssetInfo.mUuid =
			mResourceDatap->mAssetInfo.mTransactionID.makeAssetID(gAgent.getSecureSessionID());
	mResourceDatap->mAssetInfo.mType = LLAssetType::AT_TEXTURE;
	mResourceDatap->mPreferredLocation = LLFolderType::EType(-2);
	mResourceDatap->mAssetInfo.mCreatorID = gAgentID;
	mResourceDatap->mAssetInfo.setName("screenshot_name");
	mResourceDatap->mAssetInfo.setDescription("screenshot_descr");

	// Store in cache
	LLFileSystem j2c_file(mResourceDatap->mAssetInfo.mUuid,
						  LLFileSystem::OVERWRITE);
	j2c_file.write(j2cp->getData(), j2cp->getDataSize());

	// Store in the image list
	LLPointer<LLViewerFetchedTexture> image_in_list =
		LLViewerTextureManager::getFetchedTexture(mResourceDatap->mAssetInfo.mUuid);
	image_in_list->createGLTexture(0, mImageRaw, 0, true);
	// *HACK: mark this local image as a missing asset so that the viewer does
	// not try to fetch it from the server. *TODO: find out why the texture
	// fetcher never stops to try and fetch it when it already failed to get it
	// once...
	image_in_list->setIsMissingAsset();

	// The texture picker then uses that texture
	LLTextureCtrl* texctrlp = getChild<LLTextureCtrl>("screenshot");
	if (texctrlp)
	{
		texctrlp->setImageAssetID(mResourceDatap->mAssetInfo.mUuid);
		texctrlp->setDefaultImageAssetID(mResourceDatap->mAssetInfo.mUuid);
		texctrlp->setCaption("Screenshot");
	}
}

void LLFloaterReporter::uploadImage()
{
	if (!gAssetStoragep)
	{
		llwarns << "No valid asset storage. Aborted." << llendl;
		return;
	}

	llinfos << "*** Uploading: " << llendl;
	llinfos << "Type: "
			<< LLAssetType::lookup(mResourceDatap->mAssetInfo.mType)
			<< llendl;
	llinfos << "UUID: " << mResourceDatap->mAssetInfo.mUuid << llendl;
	llinfos << "Name: " << mResourceDatap->mAssetInfo.getName() << llendl;
	llinfos << "Desc: " << mResourceDatap->mAssetInfo.getDescription()
			<< llendl;

	gAssetStoragep->storeAssetData(mResourceDatap->mAssetInfo.mTransactionID,
								   mResourceDatap->mAssetInfo.mType,
								   uploadDoneCallback, (void*)mResourceDatap,
								   true);
}

// StoreAssetData callback (fixed)
//static
void LLFloaterReporter::uploadDoneCallback(const LLUUID& uuid, void* user_data,
										   S32 result, LLExtStat ext_status)
{
	LLUploadDialog::modalUploadFinished();

	LLResourceData* data = (LLResourceData*)user_data;

	if (result < 0)
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
		gNotifications.add("ErrorUploadingReportScreenshot", args);
		llwarns << "There was a problem uploading a report screenshot due to the following reason: "
				<< args["REASON"].asString() << llendl;
		return;
	}

	if (data->mPreferredLocation != LLResourceData::INVALID_LOCATION)
	{
		llwarns << "Unknown report type: " << data->mPreferredLocation
				<< llendl;
	}

	LLFloaterReporter* self = findInstance();
	if (self)
	{
		self->mScreenID = uuid;
		llinfos << "Got screen shot " << uuid << llendl;
		self->sendReportViaLegacy(self->gatherReport());
		self->close();
	}
}

void LLFloaterReporter::setPosBox(const LLVector3d& pos)
{
	mPosition.set(pos);
	std::string pos_string = llformat("{%.1f, %.1f, %.1f}", mPosition.mV[VX],
									  mPosition.mV[VY], mPosition.mV[VZ]);
	childSetText("pos_field", pos_string);
}
