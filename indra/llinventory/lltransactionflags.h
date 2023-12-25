/**
 * @file lltransactionflags.h
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

#ifndef LL_LLTRANSACTIONFLAGS_H
#define LL_LLTRANSACTIONFLAGS_H

class LLUUID;

typedef U8 TransactionFlags;

constexpr U8 TRANSACTION_FLAGS_NONE = 0;
constexpr U8 TRANSACTION_FLAG_SOURCE_GROUP = 1;
constexpr U8 TRANSACTION_FLAG_DEST_GROUP = 2;
constexpr U8 TRANSACTION_FLAG_OWNER_GROUP = 4;
constexpr U8 TRANSACTION_FLAG_SIMULTANEOUS_CONTRIBUTION = 8;
constexpr U8 TRANSACTION_FLAG_SIMULTANEOUS_CONTRIBUTION_REMOVAL = 16;

// very simple helper functions
TransactionFlags pack_transaction_flags(bool is_source_group,
										bool is_dest_group);

// stupid helper functions which should be replaced with some kind of
// internationalizeable message.
std::string build_transfer_message_to_source(S32 amount,
											 const LLUUID& source_id,
											 const LLUUID& dest_id,
											 const std::string& dest_name,
											 S32 transaction_type,
											 const std::string& description);

std::string build_transfer_message_to_destination(S32 amount,
												  const LLUUID& dest_id,
												  const LLUUID& source_id,
                                                  const std::string& source_name,
												  S32 transaction_type,
												  const std::string& description);

#endif // LL_LLTRANSACTIONFLAGS_H
