/** 
 * @file llviewerprecompiledheaders.h
 * @brief precompiled headers for newview project
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERPRECOMPILEDHEADERS_H
#define LL_LLVIEWERPRECOMPILEDHEADERS_H

// This file MUST be the first one included by each .cpp file in viewer. It is
// used to precompile headers for improved build speed.

#include "linden_common.h"

#include <algorithm>
#include <functional>

// Headers from llcommon
#include "indra_constants.h"
#include "llassettype.h"
#include "llcriticaldamp.h"
#include "llframetimer.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llsingleton.h"
#include "llstrider.h"
#include "llstring.h"
#include "lltimer.h"

// Headers from llmath
#include "llmath.h"
#include "llcamera.h"
#include "llcoord.h"
#include "llplane.h"
#include "llquantize.h"
#include "llrand.h"
#include "llrect.h"
#include "llmatrix4.h"
#include "llvector2.h"
#include "llcolor3.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llcolor4.h"
#include "llcolor4u.h"
#include "llvector4.h"
#include "llxform.h"

#endif
