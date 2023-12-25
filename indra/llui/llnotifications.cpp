/**
* @file llnotifications.cpp
* @brief Non-UI queue manager for keeping a prioritized list of notifications
*
* $LicenseInfo:firstyear=2008&license=viewergpl$
*
* Copyright (c) 2008-2009, Linden Research, Inc.
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

#include <algorithm>
#include <regex>

#include "llnotifications.h"

#include "lldir.h"
#include "llsdserialize.h"
#include "lluictrlfactory.h"

LLNotifications gNotifications;

const std::string NOTIFICATION_PERSIST_VERSION = "0.93";
const std::string TEMPLATES_FILE = "notifications.xml";
const std::string PERSISTENT_NOTIF_XML_FILE =
	"open_notifications_coolvlviewer.xml";

// Local channel for notification history
class LLNotificationHistoryChannel : public LLNotificationChannel
{
protected:
	LOG_CLASS(LLNotificationHistoryChannel);

public:
	LLNotificationHistoryChannel(const std::string& filename)
	:	LLNotificationChannel("History", "Visible", &historyFilter,
							  LLNotificationComparators::orderByUUID()),
		mFileName(filename)
	{
		connectChanged(boost::bind(&LLNotificationHistoryChannel::historyHandler,
								   this, _1));
		loadPersistentNotifications();
	}

private:
	bool historyHandler(const LLSD& payload)
	{
		// We ignore "load" messages, but rewrite the persistence file on any
		// other
		std::string sigtype = payload["sigtype"];
		if (sigtype != "load")
		{
			savePersistentNotifications();
		}
		return false;
	}

	// The history channel gets all notifications except those that have been
	// cancelled
	LL_INLINE static bool historyFilter(LLNotificationPtr notifp)
	{
		return notifp && !notifp->isCancelled();
	}

	void savePersistentNotifications()
	{
		LL_DEBUGS("Notifications") << "Saving open notifications to "
								   << mFileName << LL_ENDL;

		llofstream notify_file(mFileName.c_str());
		if (!notify_file.is_open())
		{
			llwarns << "Failed to open " << mFileName << llendl;
			return;
		}

		LLSD output;
		output["version"] = NOTIFICATION_PERSIST_VERSION;
		LLSD& data = output["data"];

		for (LLNotificationSet::iterator it = mItems.begin(),
										 end = mItems.end();
			 it != end; ++it)
		{
			LLNotificationPtr n = *it;
			if (n && gNotifications.templateExists(n->getName()))
			{
				// Only store notifications flagged as persisting
				LLNotificationTemplatePtr t = gNotifications.getTemplate(n->getName());
				if (t && t->mPersist)
				{
					data.append(n->asLLSD());
				}
			}
		}

		LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
		formatter->format(output, notify_file, LLSDFormatter::OPTIONS_PRETTY);
	}

	void loadPersistentNotifications()
	{
		llinfos << "Loading open notifications from " << mFileName << llendl;

		llifstream notify_file(mFileName.c_str());
		if (!notify_file.is_open())
		{
			llwarns << "Failed to open " << mFileName << llendl;
			return;
		}

		LLSD input;
		LLPointer<LLSDParser> parser = new LLSDXMLParser();
		if (parser->parse(notify_file, input,
						  LLSDSerialize::SIZE_UNLIMITED) < 0)
		{
			llwarns << "Failed to parse open notifications" << llendl;
			return;
		}

		if (input.isUndefined()) return;

		std::string version = input["version"];
		if (version != NOTIFICATION_PERSIST_VERSION)
		{
			llwarns << "Bad open notifications version: " << version << llendl;
			return;
		}

		LLSD& data = input["data"];
		if (data.isUndefined()) return;

		for (LLSD::array_const_iterator it = data.beginArray(),
										end = data.endArray();
			 it != end; ++it)
		{
			gNotifications.add(LLNotificationPtr(new LLNotification(*it)));
		}
	}

	//virtual
	void onDelete(LLNotificationPtr notifp)
	{
		// We want to keep deleted notifications in our log
		mItems.insert(notifp);
	}

private:
	std::string mFileName;
};

bool filterIgnoredNotifications(LLNotificationPtr notification)
{
	LLNotificationFormPtr form = notification->getForm();
	// Check to see if the user wants to ignore this alert
	if (form && form->getIgnoreType() != LLNotificationForm::IGNORE_NO)
	{
		LLControlGroup* config = LLUI::sConfigGroup;
		return config && config->getWarning(notification->getName());
	}

	return true;
}

bool handleIgnoredNotification(const LLSD& payload)
{
	if (payload["sigtype"].asString() != "add")
	{
		return false;
	}

	LLNotificationPtr notifp = gNotifications.find(payload["id"].asUUID());
	if (!notifp)
	{
		return false;
	}

	LLNotificationFormPtr form = notifp->getForm();
	LLSD response;
	switch (form->getIgnoreType())
	{
		case LLNotificationForm::IGNORE_WITH_DEFAULT_RESPONSE:
			response =
				notifp->getResponseTemplate(LLNotification::WITH_DEFAULT_BUTTON);
			break;

		case LLNotificationForm::IGNORE_WITH_LAST_RESPONSE:
			if (LLUI::sIgnoresGroup)
			{
				std::string cname = "Default" + notifp->getName();
				response = LLUI::sIgnoresGroup->getLLSD(cname.c_str());
			}
			if (response.isUndefined() || !response.isMap() ||
				response.beginMap() == response.endMap())
			{
				// Invalid saved response: let's use something that we can
				// trust
				response =
					notifp->getResponseTemplate(LLNotification::WITH_DEFAULT_BUTTON);
			}
			break;

		case LLNotificationForm::IGNORE_SHOW_AGAIN:
			break;

		default:
			return false;
	}
	notifp->setIgnored(true);
	notifp->respond(response);
	return true; 	// Do not process this item any further
}

namespace LLNotificationFilters
{
	// a sample filter
	bool includeEverything(LLNotificationPtr p)
	{
		return true;
	}
};

LLNotificationForm::LLNotificationForm()
:	mFormData(LLSD::emptyArray()),
	mIgnore(IGNORE_NO)
{
}

LLNotificationForm::LLNotificationForm(const std::string& name,
									   const LLXMLNodePtr xml_node)
:	mFormData(LLSD::emptyArray()),
	mIgnore(IGNORE_NO)
{
	if (!xml_node->hasName("form"))
	{
		llwarns << "Bad xml node for form: " << xml_node->getName() << llendl;
	}

	LLControlGroup* ignores = LLUI::sIgnoresGroup;
	LLXMLNodePtr child = xml_node->getFirstChild();
	std::string cname;
	while (child)
	{
		child = gNotifications.checkForXMLTemplate(child);

		LLSD item_entry;
		std::string element_name = child->getName()->mString;

		if (element_name == "ignore" && ignores)
		{
			bool save_option = false;
			child->getAttributeBool("save_option", save_option);
			if (!save_option)
			{
				mIgnore = IGNORE_WITH_DEFAULT_RESPONSE;
			}
			else
			{
				// remember last option chosen by user and automatically
				// respond with that in the future
				mIgnore = IGNORE_WITH_LAST_RESPONSE;
				cname = "Default" + name;
				ignores->declareLLSD(cname.c_str(), "",
									 "Default response for notification " +
									 name);
			}
			child->getAttributeString("text", mIgnoreMsg);
			ignores->addWarning(name);
		}
		else
		{
			// Flatten xml form entry into single LLSD map with type==name
			item_entry["type"] = element_name;
			for (LLXMLAttribList::iterator it = child->mAttributes.begin(),
										   end = child->mAttributes.end();
				 it != end; ++it)
			{
				if (it->second)	// Paranoia
				{
					std::string name(it->second->getName()->mString);
					item_entry[name] = it->second->getValue();
				}
			}
			item_entry["value"] = child->getTextContents();
			mFormData.append(item_entry);
		}

		child = child->getNextSibling();
	}
}

LLNotificationForm::LLNotificationForm(const LLSD& sd)
{
	if (sd.isArray())
	{
		mFormData = sd;
	}
	else
	{
		llwarns << "Invalid form data " << sd << llendl;
		mFormData = LLSD::emptyArray();
	}
}

LLSD LLNotificationForm::getElement(const std::string& element_name)
{
	for (LLSD::array_const_iterator it = mFormData.beginArray(),
									end = mFormData.endArray();
		 it != end; ++it)
	{
		if ((*it)["name"].asString() == element_name)
		{
			return *it;
		}
	}

	return LLSD();
}

bool LLNotificationForm::hasElement(const std::string& element_name)
{
	for (LLSD::array_const_iterator it = mFormData.beginArray(),
									end = mFormData.endArray();
		 it != end; ++it)
	{
		if ((*it)["name"].asString() == element_name)
		{
			return true;
		}
	}

	return false;
}

void LLNotificationForm::addElement(const std::string& type,
									const std::string& name, const LLSD& value)
{
	LLSD element;
	element["type"] = type;
	element["name"] = name;
	element["text"] = name;
	element["value"] = value;
	element["index"] = mFormData.size();
	mFormData.append(element);
}

void LLNotificationForm::append(const LLSD& sub_form)
{
	if (sub_form.isArray())
	{
		for (LLSD::array_const_iterator it = sub_form.beginArray(),
										end = sub_form.endArray();
			 it != end; ++it)
		{
			mFormData.append(*it);
		}
	}
}

void LLNotificationForm::formatElements(const LLSD& substitutions)
{
	for (LLSD::array_iterator it = mFormData.beginArray(),
							  end = mFormData.endArray();
		 it != end; ++it)
	{
		// format "text" component of each form element
		if (it->has("text"))
		{
			std::string text = (*it)["text"].asString();
			text = LLNotification::format(text, substitutions);
			(*it)["text"] = text;
		}
		if ((*it)["type"].asString() == "text" && it->has("value"))
		{
			std::string value = (*it)["value"].asString();
			value = LLNotification::format(value, substitutions);
			(*it)["value"] = value;
		}
	}
}

std::string LLNotificationForm::getDefaultOption()
{
	for (LLSD::array_const_iterator it = mFormData.beginArray(),
									end = mFormData.endArray();
		 it != end; ++it)
	{
		if ((*it)["default"])
		{
			return (*it)["name"].asString();
		}
	}

	return "";
}

LLNotificationTemplate::LLNotificationTemplate()
:	mExpireSeconds(0),
	mExpireOption(-1),
	mURLOption(-1),
	mUnique(false),
	mPriority(NOTIFICATION_PRIORITY_NORMAL)
{
	mForm = LLNotificationFormPtr(new LLNotificationForm());
}

LLNotification::LLNotification(const LLNotification::Params& p)
:	mTimestamp(p.timestamp),
	mSubstitutions(p.substitutions),
	mPayload(p.payload),
	mExpiresAt(0),
	mResponseFunctorName(p.functor_name),
	mTemporaryResponder(p.mTemporaryResponder),
	mRespondedTo(false),
	mPriority(p.priority),
	mCancelled(false),
	mIgnored(false)
{
	mId.generate();
	init(p.name, p.form_elements);
}

LLNotification::LLNotification(const LLSD& sd)
:	mTemporaryResponder(false),
	mRespondedTo(false),
	mCancelled(false),
	mIgnored(false)
{
	mId.generate();
	mSubstitutions = sd["substitutions"];
	mPayload = sd["payload"];
	mTimestamp = sd["time"];
	mExpiresAt = sd["expiry"];
	mPriority = (ENotificationPriority)sd["priority"].asInteger();
	mResponseFunctorName = sd["responseFunctor"].asString();
	std::string templatename = sd["name"].asString();
	init(templatename, LLSD());
	// replace form with serialized version
	mForm = LLNotificationFormPtr(new LLNotificationForm(sd["form"]));
}

LLSD LLNotification::asLLSD()
{
	LLSD output;
	output["name"] = mTemplatep->mName;
	output["form"] = getForm()->asLLSD();
	output["substitutions"] = mSubstitutions;
	output["payload"] = mPayload;
	output["time"] = mTimestamp;
	output["expiry"] = mExpiresAt;
	output["priority"] = (S32)mPriority;
	output["responseFunctor"] = mResponseFunctorName;
	return output;
}

void LLNotification::update()
{
	gNotifications.update(shared_from_this());
}

void LLNotification::updateFrom(LLNotificationPtr other)
{
	// can only update from the same notification type
	if (mTemplatep != other->mTemplatep) return;

	// NOTE: do NOT change the ID, since it is the key to
	// this given instance, just update all the metadata
	//mId = other->mId;

	mPayload = other->mPayload;
	mSubstitutions = other->mSubstitutions;
	mTimestamp = other->mTimestamp;
	mExpiresAt = other->mExpiresAt;
	mCancelled = other->mCancelled;
	mIgnored = other->mIgnored;
	mPriority = other->mPriority;
	mForm = other->mForm;
	mResponseFunctorName = other->mResponseFunctorName;
	mRespondedTo = other->mRespondedTo;
	mTemporaryResponder = other->mTemporaryResponder;

	update();
}

LLSD LLNotification::getResponseTemplate(EResponseTemplateType type)
{
	LLSD response = LLSD::emptyMap();
	for (S32 element_idx = 0; element_idx < mForm->getNumElements();
		 ++element_idx)
	{
		LLSD element = mForm->getElement(element_idx);
		if (element.has("name"))
		{
			response[element["name"].asString()] = element["value"];
		}

		if (type == WITH_DEFAULT_BUTTON && element["default"].asBoolean())
		{
			response[element["name"].asString()] = true;
		}
	}
	return response;
}

//static
S32 LLNotification::getSelectedOption(const LLSD& notification,
									  const LLSD& response)
{
	LLNotificationForm form(notification["form"]);

	for (S32 element_idx = 0; element_idx < form.getNumElements();
		 ++element_idx)
	{
		LLSD element = form.getElement(element_idx);

		// only look at buttons
		if (element["type"].asString() == "button"
			&& response[element["name"].asString()].asBoolean())
		{
			return element["index"].asInteger();
		}
	}

	return -1;
}

//static
std::string LLNotification::getSelectedOptionName(const LLSD& response)
{
	for (LLSD::map_const_iterator response_it = response.beginMap();
		 response_it != response.endMap(); ++response_it)
	{
		if (response_it->second.isBoolean() && response_it->second.asBoolean())
		{
			return response_it->first;
		}
	}
	return "";
}

void LLNotification::respond(const LLSD& response, bool save)
{
	mRespondedTo = true;

	// Call the functor
	LLNotificationFunctorRegistry::ResponseFunctor functor =
		LLNotificationFunctorRegistry::getInstance()->getFunctor(mResponseFunctorName);
	functor(asLLSD(), response);

	if (mTemporaryResponder)
	{
		LLNotificationFunctorRegistry::getInstance()->unregisterFunctor(mResponseFunctorName);
		mResponseFunctorName = "";
		mTemporaryResponder = false;
	}

	if (save)
	{
		LLControlGroup* ignores = LLUI::sIgnoresGroup;
		if (ignores && mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			ignores->setWarning(getName(), !mIgnored);
			if (mIgnored &&
				mForm->getIgnoreType() == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
			{
				std::string cname = "Default" + getName();
				ignores->setLLSD(cname.c_str(), response);
			}
		}
	}

	update();
}

void LLNotification::setResponseFunctor(const std::string& functor_name)
{
	if (mTemporaryResponder)
	{
		// Get rid of the old one
		LLNotificationFunctorRegistry::getInstance()->unregisterFunctor(mResponseFunctorName);
	}
	mResponseFunctorName = functor_name;
	mTemporaryResponder = false;
}

bool LLNotification::payloadContainsAll(const std::vector<std::string>& required_fields) const
{
	for (std::vector<std::string>::const_iterator
			required_fields_it = required_fields.begin();
		 required_fields_it != required_fields.end(); ++required_fields_it)
	{
		std::string required_field_name = *required_fields_it;
		if (!getPayload().has(required_field_name))
		{
			return false; // a required field was not found
		}
	}
	return true; // all required fields were found
}

bool LLNotification::isEquivalentTo(LLNotificationPtr that) const
{
	if (this->mTemplatep->mName != that->mTemplatep->mName)
	{
		return false; // must have the same template name or forget it
	}
	if (this->mTemplatep->mUnique)
	{
		// Highlander bit set there can only be one of these
		return this->payloadContainsAll(that->mTemplatep->mUniqueContext) &&
			   that->payloadContainsAll(this->mTemplatep->mUniqueContext);
	}
	return false;
}

void LLNotification::init(const std::string& template_name,
						  const LLSD& form_elements)
{
	mTemplatep = gNotifications.getTemplate(template_name);
	if (!mTemplatep) return;

	// Add default substitutions
	// *TODO: change this to read from the translatable strings file
	mSubstitutions["SECOND_LIFE"] = "Second Life";
	mSubstitutions["_URL"] = getURL();
	mSubstitutions["_NAME"] = template_name;

	mForm = LLNotificationFormPtr(new LLNotificationForm(*mTemplatep->mForm));
	mForm->append(form_elements);

	// Apply substitution to form labels
	mForm->formatElements(mSubstitutions);

	LLDate rightnow = LLDate::now();
	if (mTemplatep->mExpireSeconds)
	{
		mExpiresAt = LLDate(rightnow.secondsSinceEpoch() +
							mTemplatep->mExpireSeconds);
	}

	if (mPriority == NOTIFICATION_PRIORITY_UNSPECIFIED)
	{
		mPriority = mTemplatep->mPriority;
	}
}

std::string LLNotification::summarize() const
{
	std::string s = "Notification(" + getName() + ") : ";

	if (mTemplatep)
	{
		s += mTemplatep->mMessage;
	}

	// should also include timestamp and expiration time (but probably not
	// payload)

	return s;
}

//static
std::string LLNotification::format(const std::string& s,
								   const LLSD& substitutions)
{
	if (s.empty() || !substitutions.isMap() ||
		s.find('[') == std::string::npos)
	{
		return s;
	}

	std::string::const_iterator start = s.begin();
	std::string::const_iterator end = s.end();
	std::ostringstream output;

	try
	{
		// Match strings like [NAME]
		const std::regex key("\\[([0-9_A-Z]+)\\]");
		std::smatch match;
		while (std::regex_search(start, end, match, key))
		{
			bool found_replacement = false;
			std::string replacement;

			// See if we have a replacement for the bracketed string (without
			// the brackets) test first using has() because if we just look up
			// with operator[] we get back an empty string even if the value is
			// missing. We want to distinguish between missing replacements and
			// deliberately empty replacement strings.
			if (substitutions.has(std::string(match[1].first,
											  match[1].second)))
			{
				replacement = substitutions[std::string(match[1].first,
											match[1].second)].asString();
				found_replacement = true;
			}
			// If not, see if there is one WITH brackets
			else if (substitutions.has(std::string(match[0].first,
												   match[0].second)))
			{
				replacement = substitutions[std::string(match[0].first,
											match[0].second)].asString();
				found_replacement = true;
			}

			if (found_replacement)
			{
				output << std::string(start, match[0].first) << replacement;
			}
			else
			{
				// We had no replacement, so leave the string we searched for,
				// so that it gets noticed by QA: "stuff [NAME_NOT_FOUND]" is
				// output.
				output << std::string(start, match[0].second);
			}

			// Update search position
			start = match[0].second;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	// Send the remainder of the string (with no further matches for bracketed
	// names)
	output << std::string(start, end);

	return output.str();
}

std::string LLNotification::getMessage() const
{
	// All our callers cache this result, so it gives us more flexibility to do
	// the substitution at call time rather than attempting to cache it in the
	// notification
	return mTemplatep ? format(mTemplatep->mMessage, mSubstitutions) : "";
}

std::string LLNotification::getLabel() const
{
	return mTemplatep ? format(mTemplatep->mLabel, mSubstitutions) : "";
}

///////////////////////////////////////////////////////////////////////////////
// LLNotificationChannel implementation
///////////////////////////////////////////////////////////////////////////////

void LLNotificationChannelBase::connectChanged(const LLStandardNotificationSignal::slot_type& slot)
{
	// When someone wants to connect to a channel, we first throw them all of
	// the notifications that are already in the channel we use a special
	// signal called "load" in case the channel wants to care only about new
	// notifications
	for (LLNotificationSet::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		LLNotificationPtr n = *it;
		if (n)	// Paranoia
		{
			slot(LLSD().with("sigtype", "load").with("id", n->getID()));
		}
	}

	// And then connect the signal so that all future notifications will also
	// be forwarded.
	mChanged.connect(slot);
}

void LLNotificationChannelBase::connectPassedFilter(const LLStandardNotificationSignal::slot_type& slot)
{
	// These two filters only fire for notifications added after the current
	// one, because they do not participate in the hierarchy.
	mPassedFilter.connect(slot);
}

void LLNotificationChannelBase::connectFailedFilter(const LLStandardNotificationSignal::slot_type& slot)
{
	mFailedFilter.connect(slot);
}

// External call, conforms to our standard signature
bool LLNotificationChannelBase::updateItem(const LLSD& payload)
{
	// First check to see if it is in the master list
	LLNotificationPtr notifp = gNotifications.find(payload["id"]);
	return notifp && updateItem(payload, notifp);
}

// FIX QUIT NOT WORKING

// Internal call, for use in avoiding lookup
bool LLNotificationChannelBase::updateItem(const LLSD& payload,
										   LLNotificationPtr notifp)
{
	std::string cmd = payload["sigtype"];

	bool was_found = mItems.find(notifp) != mItems.end();
	bool passes_filter = mFilter(notifp);

	// First, we offer the result of the filter test to the simple signals for
	// pass/fail. One of these is guaranteed to be called. If either signal
	// returns true, the change processing is NOT performed (so do not return
	// true unless you know what you're doing !).
	bool abort_processing = false;
	if (passes_filter)
	{
		abort_processing = mPassedFilter(payload);
	}
	else
	{
		abort_processing = mFailedFilter(payload);
	}

	if (abort_processing)
	{
		return true;
	}

	if (cmd == "load")
	{
		// Should be no reason we'd ever get a load if we already have it if
		// passes filter send a load message, else do nothing
		llassert(!was_found);
		if (passes_filter)
		{
			// not in our list, add it and say so
			mItems.insert(notifp);
			abort_processing = mChanged(payload);
			onLoad(notifp);
		}
	}
	else if (cmd == "change")
	{
		// If it passes the filter now and was found, we just send a change
		// message.
		// If it passes the filter now and was not found, we have to add it.
		// If it does not pass the filter and was not found, we do nothing.
		// If it does not pass the filter and was found, we need to delete it.
		if (passes_filter)
		{
			if (was_found)
			{
				// It already existed, so this is a change since it changed in
				// place, all we have to do is resend the signal
				abort_processing = mChanged(payload);
				onChange(notifp);
			}
			else
			{
				// Not in our list, add it and say so
				mItems.insert(notifp);
				// Our payload is const, so make a copy before changing it
				LLSD newpayload = payload;
				newpayload["sigtype"] = "add";
				abort_processing = mChanged(newpayload);
				onChange(notifp);
			}
		}
		else if (was_found)
		{
			// It already existed, so this is a delete
			mItems.erase(notifp);
			// Our payload is const, so make a copy before changing it
			LLSD newpayload = payload;
			newpayload["sigtype"] = "delete";
			abort_processing = mChanged(newpayload);
			onChange(notifp);
		}
		// Did not pass, not on our list, do nothing
	}
	else if (cmd == "add")
	{
		// Should be no reason we would ever get an add if we already have it;
		// if passes the filter send an add message, else do nothing
		llassert(!was_found);
		if (passes_filter)
		{
			// Not in our list, add it and say so
			mItems.insert(notifp);
			abort_processing = mChanged(payload);
			onAdd(notifp);
		}
	}
	else if (cmd == "delete")
	{
		// If we have it in our list, pass on the delete, then delete it, else
		// do nothing
		if (was_found)
		{
			abort_processing = mChanged(payload);
			mItems.erase(notifp);
			onDelete(notifp);
		}
	}
	return abort_processing;
}

//static
LLNotificationChannelPtr LLNotificationChannel::buildChannel(const std::string& name,
															 const std::string& parent,
															 LLNotificationFilter filter,
															 LLNotificationComparator comparator)
{
	// Note: this is not a leak; notifications are self-registering. This
	// factory helps to prevent excess deletions by making sure all smart
	// pointers to notification channels come from the same source
	new LLNotificationChannel(name, parent, filter, comparator);
	return gNotifications.getChannel(name);
}

LLNotificationChannel::LLNotificationChannel(const std::string& name,
											 const std::string& parent,
											 LLNotificationFilter filter,
											 LLNotificationComparator comparator)
:	LLNotificationChannelBase(filter, comparator),
	mName(name),
	mParent(parent)
{
	// Store myself in the channel map
	gNotifications.addChannel(LLNotificationChannelPtr(this));

	// Bind to notification broadcast
	if (parent.empty())
	{
		gNotifications.connectChanged(boost::bind(&LLNotificationChannelBase::updateItem,
												  this, _1));
	}
	else
	{
		LLNotificationChannelPtr p = gNotifications.getChannel(parent);
		LLStandardNotificationSignal::slot_type f =
			boost::bind(&LLNotificationChannelBase::updateItem, this, _1);
		p->connectChanged(f);
	}
}

void LLNotificationChannel::setComparator(LLNotificationComparator comparator)
{
	mComparator = comparator;
	LLNotificationSet s2(mComparator);
	s2.insert(mItems.begin(), mItems.end());
	mItems.swap(s2);

	// Notify clients that we've been resorted
	mChanged(LLSD().with("sigtype", "sort"));
}

std::string LLNotificationChannel::summarize()
{
	std::string s = "Channel '" + mName + "'\n  ";
	for (LLNotificationChannel::Iterator it = begin(); it != end(); ++it)
	{
		s += (*it)->summarize();
		s += "\n  ";
	}
	return s;
}

///////////////////////////////////////////////////////////////////////////////
// LLNotifications implementation
///////////////////////////////////////////////////////////////////////////////

LLNotifications::LLNotifications()
:	LLNotificationChannelBase(LLNotificationFilters::includeEverything,
							  LLNotificationComparators::orderByUUID())
{
}

// Must be called once before notifications are used.
void LLNotifications::initClass()
{
	llinfos << "initializing..." << llendl;
	loadTemplates();
	createDefaultChannels();
}

// The expiration channel gets all notifications that are cancelled
bool LLNotifications::expirationFilter(LLNotificationPtr notifp)
{
	return notifp->isCancelled() || notifp->isRespondedTo();
}

bool LLNotifications::expirationHandler(const LLSD& payload)
{
	if (payload["sigtype"].asString() != "delete")
	{
		// Anything added to this channel actually should be deleted from the
		// master
		cancel(find(payload["id"]));
		return true;	// Do not process this item any further
	}

	return false;
}

bool LLNotifications::uniqueFilter(LLNotificationPtr notifp)
{
	if (!notifp->hasUniquenessConstraints())
	{
		return true;
	}

	// Check against existing unique notifications
	for (LLNotificationMap::iterator
			it = mUniqueNotifications.find(notifp->getName()),
			end = mUniqueNotifications.end();
		 it != end; ++it)
	{
		LLNotificationPtr existing_notification = it->second;
		if (notifp != existing_notification &&
			notifp->isEquivalentTo(existing_notification))
		{
			return false;
		}
	}

	return true;
}

bool LLNotifications::uniqueHandler(const LLSD& payload)
{
	LLNotificationPtr notifp = gNotifications.find(payload["id"].asUUID());
	if (notifp && notifp->hasUniquenessConstraints())
	{
		if (payload["sigtype"].asString() == "add")
		{
			// Not a duplicate according to uniqueness criteria, so we keep it
			// and store it for future uniqueness checks
			mUniqueNotifications.insert(std::make_pair(notifp->getName(),
													   notifp));
		}
		else if (payload["sigtype"].asString() == "delete")
		{
			mUniqueNotifications.erase(notifp->getName());
		}
	}

	return false;
}

bool LLNotifications::failedUniquenessTest(const LLSD& payload)
{
	LLNotificationPtr notifp = gNotifications.find(payload["id"].asUUID());
	if (!notifp || !notifp->hasUniquenessConstraints())
	{
		return false;
	}

	// Check against existing unique notifications
	for (LLNotificationMap::iterator
			it = mUniqueNotifications.find(notifp->getName()),
			end = mUniqueNotifications.end();
		 it != end; ++it)
	{
		LLNotificationPtr existing_notification = it->second;
		if (existing_notification && notifp != existing_notification &&
			notifp->isEquivalentTo(existing_notification))
		{
			// Copy notification instance data over to oldest instance of this
			// unique notification and update it
			existing_notification->updateFrom(notifp);
			// Then delete the new one
			notifp->cancel();
		}
	}

	return false;
}

void LLNotifications::addChannel(LLNotificationChannelPtr pChan)
{
	mChannels[pChan->getName()] = pChan;
}

LLNotificationChannelPtr LLNotifications::getChannel(const std::string& chan_name)
{
	ChannelMap::iterator p = mChannels.find(chan_name);
	if (p == mChannels.end())
	{
		llerrs << "Did not find channel named " << chan_name << llendl;
	}
	return p->second;
}

void LLNotifications::createDefaultChannels()
{
	// Now construct the various channels AFTER loading the notifications,
	// because the history channel is going to rewrite the stored notifications
	// file
	LLNotificationChannel::buildChannel("Expiration", "",
										boost::bind(&LLNotifications::expirationFilter,
													this, _1));
	LLNotificationChannel::buildChannel("Unexpired", "",
										!boost::bind(&LLNotifications::expirationFilter,
													 this, _1)); // use negated bind
	LLNotificationChannel::buildChannel("Unique", "Unexpired",
		boost::bind(&LLNotifications::uniqueFilter, this, _1));
	LLNotificationChannel::buildChannel("Ignore", "Unique",
										filterIgnoredNotifications);
	LLNotificationChannel::buildChannel("Visible", "Ignore",
										&LLNotificationFilters::includeEverything);

	// Create special history channel
	std::string filename;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
											  PERSISTENT_NOTIF_XML_FILE);
	// This is not a leak, do not worry about the empty "new"
	new LLNotificationHistoryChannel(filename);

	// Connect action methods to these channels
	getChannel("Expiration")->connectChanged(boost::bind(&LLNotifications::expirationHandler,
														 this, _1));
	getChannel("Unique")->connectChanged(boost::bind(&LLNotifications::uniqueHandler,
													 this, _1));
	getChannel("Unique")->connectFailedFilter(boost::bind(&LLNotifications::failedUniquenessTest,
														  this, _1));
	getChannel("Ignore")->connectFailedFilter(&handleIgnoredNotification);
}

bool LLNotifications::addTemplate(const std::string &name,
								  LLNotificationTemplatePtr theTemplate)
{
	if (mTemplates.count(name))
	{
		llwarns << "LLNotifications -- attempted to add template '"
				<< name << "' twice." << llendl;
		return false;
	}

	mTemplates[name] = theTemplate;
	return true;
}

LLNotificationTemplatePtr LLNotifications::getTemplate(const std::string& name)
{
	return mTemplates.count(name) ? mTemplates[name]
								  : mTemplates["MissingAlert"];
}

void LLNotifications::forceResponse(const LLNotification::Params& params,
									S32 option)
{
	LLNotificationPtr temp_notify(new LLNotification(params));
	LLSD response = temp_notify->getResponseTemplate();
	LLSD selected_item = temp_notify->getForm()->getElement(option);

	if (selected_item.isUndefined())
	{
		llwarns << "Invalid option " << option << " for notification "
				<< (std::string)params.name << llendl;
		return;
	}
	response[selected_item["name"].asString()] = true;

	temp_notify->respond(response, false);
}

LLNotifications::TemplateNames LLNotifications::getTemplateNames() const
{
	TemplateNames names;

	for (TemplateMap::const_iterator it = mTemplates.begin(),
									 end = mTemplates.end();
		 it != end; ++it)
	{
		names.emplace_back(it->first);
	}

	return names;
}

typedef std::map<std::string, std::string> StringMap;
void replaceSubstitutionStrings(LLXMLNodePtr node, StringMap& replacements)
{
	// walk the list of attributes looking for replacements
	for (LLXMLAttribList::iterator it = node->mAttributes.begin(),
								   end = node->mAttributes.end();
		 it != end; ++it)
	{
		if (!it->second) continue;	// Paranoia

		std::string value = it->second->getValue();
		if (value[0] == '$')
		{
			value.erase(0, 1);	// trim off the $
			std::string replacement;
			StringMap::const_iterator found = replacements.find(value);
			if (found != replacements.end())
			{
				replacement = found->second;
				LL_DEBUGS("Notifications") << "Value: " << value
										   << " - Replacement: "
										   << replacement << LL_ENDL;

				it->second->setValue(replacement);
			}
			else
			{
				llwarns << "Substitution Failure !  Value: "
						<< value << " - Replacement: " << replacement
						<< llendl;
			}
		}
	}

	// now walk the list of children and call this recursively.
	for (LLXMLNodePtr child = node->getFirstChild();
		 child.notNull(); child = child->getNextSibling())
	{
		replaceSubstitutionStrings(child, replacements);
	}
}

// Private to this file.
// Returns true if the template request was invalid and there is nothing else
// we can do with this node, false if you should keep processing (it may have
// replaced the contents of the node referred to)
LLXMLNodePtr LLNotifications::checkForXMLTemplate(LLXMLNodePtr item)
{
	if (item.notNull() && item->hasName("usetemplate"))
	{
		std::string replacement;
		if (item->getAttributeString("name", replacement))
		{
			StringMap replacements;
			for (LLXMLAttribList::const_iterator
					it = item->mAttributes.begin(),
					end = item->mAttributes.end();
				 it != end; ++it)
			{
				if (it->second)	// Paranoia
				{
					std::string name = it->second->getName()->mString;
					replacements[name] = it->second->getValue();
				}
			}
			if (mXmlTemplates.count(replacement))
			{
				item = LLXMLNode::replaceNode(item,
											  mXmlTemplates[replacement]);

				// walk the nodes looking for $(substitution) here and replace
				replaceSubstitutionStrings(item, replacements);
			}
			else
			{
				llwarns << "XML template lookup failure on: " << replacement
						<< llendl;
			}
		}
	}

	return item;
}

bool LLNotifications::loadTemplates()
{
	LLXMLNodePtr root;
	bool success = LLUICtrlFactory::getLayeredXMLNode(TEMPLATES_FILE, root);
	if (!success || root.isNull() || !root->hasName("notifications"))
	{
		llerrs << "Problem reading UI Notifications file: " << TEMPLATES_FILE
			   << llendl;
		return false;
	}

	clearTemplates();

	for (LLXMLNodePtr item = root->getFirstChild(); item.notNull();
		 item = item->getNextSibling())
	{
		// We do this FIRST so that item can be changed if we encounter a
		// usetemplate; we just replace the current xml node and keep
		// processing.
		item = checkForXMLTemplate(item);

		if (item->hasName("global"))
		{
			std::string global_name;
			if (item->getAttributeString("name", global_name))
			{
				mGlobalStrings[global_name] = item->getTextContents();
			}
			continue;
		}

		if (item->hasName("template"))
		{
			// store an xml template; templates must have a single node (can
			// contain other nodes)
			std::string name;
			item->getAttributeString("name", name);
			LLXMLNodePtr ptr = item->getFirstChild();
			mXmlTemplates[name] = ptr;
			continue;
		}

		if (!item->hasName("notification"))
		{
            llwarns << "Unexpected entity " << item->getName()->mString <<
                       " found in " << TEMPLATES_FILE << llendl;
			continue;
		}

		// now we know we have a notification entry, so let's build it
		LLNotificationTemplatePtr templatep(new LLNotificationTemplate());

		if (!item->getAttributeString("name", templatep->mName))
		{
			llwarns << "Unable to parse notification with no name" << llendl;
			continue;
		}

		LL_DEBUGS("Notifications") << "Parsing " << templatep->mName
								   << LL_ENDL;

		templatep->mMessage = item->getTextContents();
		templatep->mDefaultFunctor = templatep->mName;
		item->getAttributeString("type", templatep->mType);
		item->getAttributeString("icon", templatep->mIcon);
		item->getAttributeString("label", templatep->mLabel);
		item->getAttributeU32("duration", templatep->mExpireSeconds);
		item->getAttributeU32("expireOption", templatep->mExpireOption);

		std::string priority;
		item->getAttributeString("priority", priority);
		templatep->mPriority = NOTIFICATION_PRIORITY_NORMAL;
		if (!priority.empty())
		{
			if (priority == "low")
			{
				templatep->mPriority = NOTIFICATION_PRIORITY_LOW;
			}
			else if (priority == "high")
			{
				templatep->mPriority = NOTIFICATION_PRIORITY_HIGH;
			}
			else if (priority == "critical")
			{
				templatep->mPriority = NOTIFICATION_PRIORITY_CRITICAL;
			}
		}

		item->getAttributeString("functor", templatep->mDefaultFunctor);

		bool persist = false;
		item->getAttributeBool("persist", persist);
		templatep->mPersist = persist;

		std::string sound;
		item->getAttributeString("sound", sound);
		if (!sound.empty() && LLUI::sConfigGroup)
		{
			// TODO: test for bad sound effect name / missing effect
			templatep->mSoundEffect = LLUUID(LLUI::sConfigGroup->getString(sound.c_str()));
		}

		for (LLXMLNodePtr child = item->getFirstChild(); !child.isNull();
			 child = child->getNextSibling())
		{
			child = checkForXMLTemplate(child);

			// <url>
			if (child->hasName("url"))
			{
				templatep->mURL = child->getTextContents();
				child->getAttributeU32("option", templatep->mURLOption);
			}

            if (child->hasName("unique"))
            {
                templatep->mUnique = true;
                for (LLXMLNodePtr formitem = child->getFirstChild();
                     !formitem.isNull(); formitem = formitem->getNextSibling())
                {
                    if (formitem->hasName("context"))
                    {
                        std::string key;
                        formitem->getAttributeString("key", key);
                        templatep->mUniqueContext.emplace_back(key);
                        LL_DEBUGS("Notifications") << "adding " << key
												   << " to unique context"
												   << LL_ENDL;
                    }
                    else
                    {
                        llwarns << "'unique' has unrecognized sub-element "
								<< formitem->getName()->mString << llendl;
                    }
                }
            }

			// <form>
			if (child->hasName("form"))
			{
                templatep->mForm =
					LLNotificationFormPtr(new LLNotificationForm(templatep->mName,
																 child));
			}
		}
		addTemplate(templatep->mName, templatep);
	}

	return true;
}

// we provide a couple of simple add notification functions so that it is
// reasonable to create notifications in one line
LLNotificationPtr LLNotifications::add(const std::string& name,
									   const LLSD& substitutions,
									   const LLSD& payload)
{
	return add(LLNotification::Params(name).substitutions(substitutions).payload(payload));
}

LLNotificationPtr LLNotifications::add(const std::string& name,
									   const LLSD& substitutions,
									   const LLSD& payload,
									   const std::string& functor_name)
{
	return add(LLNotification::Params(name).substitutions(substitutions).payload(payload).functor_name(functor_name));
}

LLNotificationPtr LLNotifications::add(const std::string& name,
									   const LLSD& substitutions,
									   const LLSD& payload,
									   LLNotificationFunctorRegistry::ResponseFunctor functor)
{
	return add(LLNotification::Params(name).substitutions(substitutions).payload(payload).functor(functor));
}

// Generalized add method that takes a parameter block object for more complex
// instantiations
LLNotificationPtr LLNotifications::add(const LLNotification::Params& p)
{
	LLNotificationPtr notifp(new LLNotification(p));
	add(notifp);
	return notifp;
}

void LLNotifications::add(const LLNotificationPtr notifp)
{
	// First see if we already have it: if so, this is a problem
	LLNotificationSet::iterator it = mItems.find(notifp);
	if (it != mItems.end())
	{
		llwarns << "Attempted to add notification '" << notifp->getName()
				<< "' (existing notification id: " << notifp->getID()
				<< ") a second time to the master notification channel !"
				<< llendl;
		llassert(false);
		return;
	}

	updateItem(LLSD().with("sigtype", "add").with("id", notifp->getID()),
												  notifp);
}

void LLNotifications::cancel(LLNotificationPtr notifp)
{
	LLNotificationSet::iterator it = mItems.find(notifp);
	if (it == mItems.end())
	{
		llwarns << "Attempted to delete inexistent notification." << llendl;
		llassert(false);
		return;
	}
	updateItem(LLSD().with("sigtype", "delete").with("id", notifp->getID()),
			   notifp);
	notifp->cancel();
}

void LLNotifications::update(const LLNotificationPtr notifp)
{
	LLNotificationSet::iterator it = mItems.find(notifp);
	if (it != mItems.end())
	{
		updateItem(LLSD().with("sigtype", "change").with("id",
														 notifp->getID()),
														 notifp);
	}
}

LLNotificationPtr LLNotifications::find(LLUUID uuid)
{
	LLNotificationPtr target = LLNotificationPtr(new LLNotification(uuid));
	LLNotificationSet::iterator it = mItems.find(target);
	if (it == mItems.end())
	{
		LL_DEBUGS("Notifications") << "Cannot find notification '" << uuid
								   << LL_ENDL;
		return LLNotificationPtr(NULL);
	}
	else
	{
		return *it;
	}
}

void LLNotifications::forEachNotification(NotificationProcess process)
{
	std::for_each(mItems.begin(), mItems.end(), process);
}

const std::string& LLNotifications::getGlobalString(const std::string& key) const
{
	GlobalStringMap::const_iterator it = mGlobalStrings.find(key);
	if (it != mGlobalStrings.end())
	{
		return it->second;
	}
	// If we do not have the key as a global, return the key itself so that the
	// error is self-diagnosing.
	return key;
}

std::ostream& operator<<(std::ostream& s, const LLNotification& notification)
{
	s << notification.summarize();
	return s;
}
