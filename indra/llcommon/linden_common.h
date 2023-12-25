/** 
 * @file linden_common.h
 * @brief Includes common headers that are always safe to include
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

#ifndef LL_LINDEN_COMMON_H
#define LL_LINDEN_COMMON_H

// *NOTE: Please keep includes here to a minimum !  Files included here are
// included in every *.cpp file...

#include "llpreprocessor.h"

#if LL_WINDOWS
# include "llwin32headerslean.h"
# ifdef _DEBUG
#  define _CRTDBG_MAP_ALLOC
#  include <stdlib.h>
#  include <crtdbg.h>
# endif
#endif

#include <cstring>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iosfwd>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#if LL_OPENMP
# include <omp.h>
#endif

#if LL_DARWIN
// LL's boost pre-built for Darwin uses bzero()...
# include <strings.h>
#endif

#include "boost/bind.hpp"
#include "boost/function.hpp"

// Linden only libs in alpha-order
#include "llcommonmath.h"
#include "llerror.h"
#include "llfile.h"
#include "lluuid.h"

#endif
