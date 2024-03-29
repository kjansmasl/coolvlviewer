/**
 * @file llpanelgroupnotices.cpp
 * @brief A panel to display group notices.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llpanelgroupnotices.h"

#include "llbutton.h"
#include "llfontgl.h"
#include "lliconctrl.h"
#include "llinventory.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltexteditor.h"

#include "llagent.h"
#include "llinventorymodel.h"
#include "llinventoryicon.h"		// For getIconName()
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"		// For LLOfferInfo
#include "roles_constants.h"

//-----------------------------------------------------------------------------
// LLGroupDropTarget class
//
// This handy class is a simple way to drop something on another view. It
// handles drop events, always setting itself to the size of its parent.
//-----------------------------------------------------------------------------

class LLGroupDropTarget final : public LLView
{
protected:
	LOG_CLASS(LLGroupDropTarget);

public:
	LLGroupDropTarget(const std::string& name, const LLRect& rect,
					  LLPanelGroupNotices* panel, const LLUUID& group_id);

	~LLGroupDropTarget() override		{}

	void doDrop(EDragAndDropType cargo_type, void* cargo_data);

	// LLView functionality
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;
protected:
	LLPanelGroupNotices* mGroupNoticesPanel;
	LLUUID	mGroupID;
};

LLGroupDropTarget::LLGroupDropTarget(const std::string& name,
									 const LLRect& rect,
									 LLPanelGroupNotices* panel,
									 const LLUUID& group_id)
:	LLView(name, rect, false, FOLLOWS_ALL),
	mGroupNoticesPanel(panel),
	mGroupID(group_id)
{
}

void LLGroupDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
	llinfos << "No operation" << llendl;
}

bool LLGroupDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data, EAcceptance* accept,
									 std::string& tooltip_msg)
{
	if (!gAgent.hasPowerInGroup(mGroupID, GP_NOTICES_SEND))
	{
		*accept = ACCEPT_NO;
		return true;
	}

	if (!getParent())
	{
		return false;
	}

	if ((cargo_type == DAD_SETTINGS && !gAgent.hasInventorySettings()) ||
		(cargo_type == DAD_MATERIAL && !gAgent.hasInventoryMaterial()))
	{
		return false;
	}

	// Check the type
	switch (cargo_type)
	{
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
		case DAD_MESH:
#endif
		case DAD_SETTINGS:
		case DAD_MATERIAL:
		{
			LLViewerInventoryItem* inv_item = (LLViewerInventoryItem*)cargo_data;
			if (inv_item && gInventory.getItem(inv_item->getUUID()) &&
				LLToolDragAndDrop::isInventoryGroupGiveAcceptable(inv_item))
			{
				// *TODO: get multiple object transfers working
				*accept = ACCEPT_YES_COPY_SINGLE;
				if (drop)
				{
					mGroupNoticesPanel->setItem(inv_item);
				}
			}
			else
			{
				// It is not in the user's inventory (it is probably in an
				// object's contents), so disallow dragging it here; you cannot
				// give something you do not yet have.
					*accept = ACCEPT_NO;
				}
				break;
			}

		default:
			*accept = ACCEPT_NO;
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLPanelGroupNotices class
//-----------------------------------------------------------------------------

//static
LLPanelGroupNotices::instances_map_t LLPanelGroupNotices::sInstances;

LLPanelGroupNotices::LLPanelGroupNotices(const std::string& name,
										 const LLUUID& group_id)
:	LLPanelGroupTab(name,group_id),
	mInventoryItem(NULL),
	mInventoryOffer(NULL),
	mInitOK(false)
{
	sInstances[group_id] = this;
}

LLPanelGroupNotices::~LLPanelGroupNotices()
{
	sInstances.erase(mGroupID);

	if (mInventoryOffer)
	{
		// Cancel the inventory offer.
		mInventoryOffer->forceResponse(IOR_DECLINE);

		mInventoryOffer = NULL;
	}
}

//static
void* LLPanelGroupNotices::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupNotices("panel group notices", *group_id);
}

bool LLPanelGroupNotices::isVisibleByAgent()
{
	return mAllowEdit && gAgent.hasPowerInGroup(mGroupID,
												GP_NOTICES_SEND |
												GP_NOTICES_RECEIVE);
}

bool LLPanelGroupNotices::postBuild()
{
	mNoticesList = getChild<LLScrollListCtrl>("notice_list");
	mNoticesList->setCommitOnSelectionChange(true);
	mNoticesList->setCommitCallback(onSelectNotice);
	mNoticesList->setCallbackUserData(this);

	mBtnNewMessage = getChild<LLButton>("create_new_notice", true, false);
	if (mBtnNewMessage)
	{
		mBtnNewMessage->setClickedCallback(onClickNewMessage);
		mBtnNewMessage->setCallbackUserData(this);
		mBtnNewMessage->setEnabled(gAgent.hasPowerInGroup(mGroupID,
														  GP_NOTICES_SEND));
	}

	mBtnGetPastNotices = getChild<LLButton>("refresh_notices", true, false);
	if (mBtnGetPastNotices)
	{
		mBtnGetPastNotices->setClickedCallback(onClickRefreshNotices);
		mBtnGetPastNotices->setCallbackUserData(this);
	}

	// Create
	mCreateSubject = getChild<LLLineEditor>("create_subject");
	mCreateMessage = getChild<LLTextEditor>("create_message");

	mCreateInventoryName = getChild<LLLineEditor>("create_inventory_name");
	mCreateInventoryName->setTabStop(false);
	mCreateInventoryName->setEnabled(false);

	mCreateInventoryIcon = getChild<LLIconCtrl>("create_inv_icon",
												true, false);
	if (mCreateInventoryIcon)
	{
		mCreateInventoryIcon->setVisible(false);
	}

	mBtnSendMessage = getChild<LLButton>("send_notice", true, false);
	if (mBtnSendMessage)
	{
		mBtnSendMessage->setClickedCallback(onClickSendMessage);
		mBtnSendMessage->setCallbackUserData(this);
	}

	mBtnRemoveAttachment = getChild<LLButton>("remove_attachment",
											  true, false);
	if (mBtnRemoveAttachment)
	{
		mBtnRemoveAttachment->setClickedCallback(onClickRemoveAttachment);
		mBtnRemoveAttachment->setCallbackUserData(this);
		mBtnRemoveAttachment->setEnabled(false);
	}

	// View
	mViewSubject = getChild<LLLineEditor>("view_subject", true, false);
	mViewMessage = getChild<LLTextEditor>("view_message", true, false);
	if (mViewMessage)
	{
		mViewMessage->setParseHTML(true);
	}

	mViewInventoryName =  getChild<LLLineEditor>("view_inventory_name",
												 true, false);
	if (mViewInventoryName)
	{
		mViewInventoryName->setTabStop(false);
		mViewInventoryName->setEnabled(false);
	}

	mViewInventoryIcon = getChild<LLIconCtrl>("view_inv_icon", true, false);
	if (mViewInventoryIcon)
	{
		mViewInventoryIcon->setVisible(false);
	}

	mBtnOpenAttachment = getChild<LLButton>("open_attachment", true, false);
	if (mBtnOpenAttachment)
	{
		mBtnOpenAttachment->setClickedCallback(onClickOpenAttachment);
		mBtnOpenAttachment->setCallbackUserData(this);
	}

	mNoNoticesStr = getString("no_notices_text");

	mPanelCreateNotice = getChild<LLPanel>("panel_create_new_notice");
	mPanelViewNotice = getChild<LLPanel>("panel_view_past_notice");

	// Must be in front of all other UI elements.
	LLPanel* dtv = getChild<LLPanel>("drop_target");
	LLGroupDropTarget* target = new LLGroupDropTarget("drop_target",
													   dtv->getRect(),
													   this, mGroupID);
	target->setEnabled(true);
	target->setToolTip(dtv->getToolTip());

	mPanelCreateNotice->addChild(target);
	mPanelCreateNotice->removeChild(dtv, true);

	mInitOK = true;
	arrangeNoticeView(VIEW_PAST_NOTICE);

	mInitOK = LLPanelGroupTab::postBuild();

	return mInitOK;
}

void LLPanelGroupNotices::activate()
{
	if (!mInitOK) return;

	bool can_send = gAgent.hasPowerInGroup(mGroupID, GP_NOTICES_SEND);
	bool can_receive = gAgent.hasPowerInGroup(mGroupID, GP_NOTICES_RECEIVE);

	mPanelViewNotice->setEnabled(can_receive);
	mPanelCreateNotice->setEnabled(can_send);

	// Always disabled to stop direct editing of attachment names
	mCreateInventoryName->setEnabled(false);
	if (mViewInventoryName) mViewInventoryName->setEnabled(false);

	// If we can receive notices, grab them right away.
	if (can_receive)
	{
		onClickRefreshNotices(this);
	}
}

void LLPanelGroupNotices::setItem(LLPointer<LLInventoryItem> inv_item)
{
	if (!mInitOK) return;
	mInventoryItem = inv_item;

	bool item_is_multi = false;
	if (inv_item->getFlags() &
		LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS)
	{
		item_is_multi = true;
	}

	if (mCreateInventoryIcon)
	{
		std::string icon_name =
			LLInventoryIcon::getIconName(inv_item->getType(),
										 inv_item->getInventoryType(),
										 inv_item->getFlags(), item_is_multi);
		mCreateInventoryIcon->setImage(icon_name);
		mCreateInventoryIcon->setVisible(true);
	}

	std::stringstream ss;
	ss << "        " << mInventoryItem->getName();

	mCreateInventoryName->setText(ss.str());
	if (mBtnRemoveAttachment)
	{
		mBtnRemoveAttachment->setEnabled(true);
	}
}

void LLPanelGroupNotices::onClickRemoveAttachment(void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	self->mInventoryItem = NULL;
	self->mCreateInventoryName->clear();
	if (self->mCreateInventoryIcon)
	{
		self->mCreateInventoryIcon->setVisible(false);
	}
	if (self->mBtnRemoveAttachment)
	{
		self->mBtnRemoveAttachment->setEnabled(false);
	}
}

//static
void LLPanelGroupNotices::onClickOpenAttachment(void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	if (self->mInventoryOffer)
	{
		self->mInventoryOffer->forceResponse(IOR_ACCEPT);
		self->mInventoryOffer = NULL;
	}
	self->mBtnOpenAttachment->setEnabled(false);
}

void LLPanelGroupNotices::onClickSendMessage(void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	if (self->mCreateSubject->getText().empty())
	{
		// Must supply a subject
		gNotifications.add("MustSpecifyGroupNoticeSubject");
		return;
	}
	send_group_notice(self->mGroupID, self->mCreateSubject->getText(),
					  self->mCreateMessage->getText(), self->mInventoryItem);

	self->mCreateMessage->clear();
	self->mCreateSubject->clear();
	onClickRemoveAttachment(data);

	self->arrangeNoticeView(VIEW_PAST_NOTICE);
	onClickRefreshNotices(self);
}

//static
void LLPanelGroupNotices::onClickNewMessage(void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	self->arrangeNoticeView(CREATE_NEW_NOTICE);

	if (self->mInventoryOffer)
	{
		self->mInventoryOffer->forceResponse(IOR_DECLINE);
		self->mInventoryOffer = NULL;
	}

	self->mCreateSubject->clear();
	self->mCreateMessage->clear();

	if (self->mInventoryItem)
	{
		onClickRemoveAttachment(self);
	}

	// NOTE: true == do not commit on change
	self->mNoticesList->deselectAllItems(true);
}

void LLPanelGroupNotices::onClickRefreshNotices(void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	LL_DEBUGS("GroupPanel") << "Sending GroupNoticesListRequest" << LL_ENDL;

	self->mNoticesList->deleteAllItems();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("GroupNoticesListRequest");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("Data");
	msg->addUUID("GroupID", self->mGroupID);
	gAgent.sendReliableMessage();
}

//static
void LLPanelGroupNotices::processGroupNoticesListReply(LLMessageSystem* msg,
													   void** data)
{
	LLUUID group_id;
	msg->getUUID("AgentData", "GroupID", group_id);

	instances_map_t::iterator it = sInstances.find(group_id);
	if (it == sInstances.end())
	{
		llinfos << "Group Panel Notices " << group_id
				<< " no longer in existence." << llendl;
		return;
	}

	LLPanelGroupNotices* selfp = it->second;
	if (!selfp)
	{
		llinfos << "Group Panel Notices " << group_id
				<< " no longer in existence." << llendl;
		return;
	}

	selfp->processNotices(msg);
}

void LLPanelGroupNotices::processNotices(LLMessageSystem* msg)
{
	if (!mInitOK) return;

	LLUUID id;
	std::string subj, name;
	U32 timestamp;
	bool has_attachment;
	U8 asset_type;

	std::string format = gSavedSettings.getString("ShortDateFormat");
	S32 count = msg->getNumberOfBlocks("Data");
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUID("Data","NoticeID",id,i);
		if (count == 1 && id.isNull())
		{
			// Only one entry, the dummy entry.
			mNoticesList->addCommentText(mNoNoticesStr);
			mNoticesList->setEnabled(false);
			return;
		}

		msg->getString("Data","Subject", subj, i);
		msg->getString("Data","FromName", name, i);
		msg->getBool("Data","HasAttachment", has_attachment, i);
		msg->getU8("Data","AssetType", asset_type, i);
		msg->getU32("Data","Timestamp", timestamp, i);

		LLSD row;
		row["id"] = id;

		row["columns"][0]["column"] = "icon";
		if (has_attachment)
		{
			std::string icon;
			icon = LLInventoryIcon::getIconName((LLAssetType::EType)asset_type,
												LLInventoryType::IT_NONE,
												false, false);
			row["columns"][0]["type"] = "icon";
			row["columns"][0]["value"] = icon;
		}

		row["columns"][1]["column"] = "subject";
		row["columns"][1]["value"] = subj;

		row["columns"][2]["column"] = "from";
		row["columns"][2]["value"] = name;

		std::string buffer;
		row["columns"][3]["column"] = "date";
		row["columns"][3]["type"] = "date";
		row["columns"][3]["format"] = format;
		row["columns"][3]["value"] = LLDate(timestamp);

		mNoticesList->addElement(row, ADD_BOTTOM);
	}

	mNoticesList->sortByColumnIndex(3, false);
}

void LLPanelGroupNotices::onSelectNotice(LLUICtrl* ctrl, void* data)
{
	LLPanelGroupNotices* self = (LLPanelGroupNotices*)data;
	if (!self || !self->mInitOK) return;

	LLScrollListItem* item = self->mNoticesList->getFirstSelected();
	if (!item) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("GroupNoticeRequest");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("Data");
	msg->addUUID("GroupNoticeID",item->getUUID());
	gAgent.sendReliableMessage();

	LL_DEBUGS("GroupPanel") << "Item " << item->getUUID() << " selected."
							<< LL_ENDL;
}

void LLPanelGroupNotices::showNotice(const std::string& subject,
									 const std::string& message, bool,
									 const std::string& inventory_name,
									 LLOfferInfo* inventory_offer)
{
	arrangeNoticeView(VIEW_PAST_NOTICE);

	if (mViewSubject) mViewSubject->setText(subject);
	if (mViewMessage)
	{
		mViewMessage->clear();
		mViewMessage->setParseHTML(true);
		// Now we append the new text (setText() won't highlight URLs)
		mViewMessage->appendColoredText(message, false, false,
										mViewMessage->getReadOnlyFgColor());
	}

	if (mInventoryOffer)
	{
		// Cancel the inventory offer for the previously viewed notice
		mInventoryOffer->forceResponse(IOR_DECLINE);
		mInventoryOffer = NULL;
	}

	if (inventory_offer)
	{
		mInventoryOffer = inventory_offer;

		if (mViewInventoryIcon)
		{
			std::string icon_name =
				LLInventoryIcon::getIconName(mInventoryOffer->mType,
											 LLInventoryType::IT_TEXTURE,
											 0, false);
			mViewInventoryIcon->setImage(icon_name);
			mViewInventoryIcon->setVisible(true);
		}

		if (mViewInventoryName)
		{
			std::stringstream ss;
			ss << "        " << inventory_name;
			mViewInventoryName->setText(ss.str());
		}
		if (mBtnOpenAttachment)
		{
			mBtnOpenAttachment->setEnabled(true);
		}
	}
	else
	{
		if (mViewInventoryName)
		{
			mViewInventoryName->clear();
		}
		if (mViewInventoryIcon)
		{
			mViewInventoryIcon->setVisible(false);
		}
		if (mBtnOpenAttachment)
		{
			mBtnOpenAttachment->setEnabled(false);
		}
	}
}

void LLPanelGroupNotices::arrangeNoticeView(ENoticeView view_type)
{
	if (!mInitOK) return;

	if (view_type == CREATE_NEW_NOTICE)
	{
        mPanelCreateNotice->setVisible(true);
		mPanelViewNotice->setVisible(false);
	}
	else
	{
		mPanelCreateNotice->setVisible(false);
		mPanelViewNotice->setVisible(true);
		if (mBtnOpenAttachment)
		{
			mBtnOpenAttachment->setEnabled(false);
		}
	}
}
