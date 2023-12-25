/**
 * @file llerror.h
 * @brief Error message system basic functions declaration
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
 * Copyright (c) 2009-2023, Henri Beauchamp.
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

#ifndef LL_LLERROR_H
#define LL_LLERROR_H

#include <string>
#include <sstream>
#include <typeinfo>

#include "stdtypes.h"

#include "llpreprocessor.h"
#include "hbfastmap.h"

constexpr int LL_ERR_NOERR = 0;

// Notes by Henri Beauchamp: in the Cool VL Viewer, the error logging scheme
// was simplified, highly optimized, and made friendly to both programmers and
// users.
//
// First, I made the following constatations in LL's original code:
//   - There was no point at all for tagged info, warning and error messages
//     (LL_INFOS("Tag"), LL_WARNS("Tag"), LL_ERRS("Tag")) since all info and
//     warning messages are normally always logged, unless the user fiddles
//     with the app_settings/logcontrol.xml file to explicitely disable them
//     (but what the Hell for ?). LL_ERRS("Tag") by itself was totally useless,
//     since encountering any such message always involved a voluntary crash.
//   - LL_DEBUGS("Tag") is indeed useful, but LL's code did not even allow to
//     enable those messages via the UI and you had to recourse to editing
//     manually the app_settings/logcontrol.xml file before viewer launch.
//   - "Narrow tags" were only actually used in a couple of places, and did not
//     really bring anything when compared to using two different "broad tags",
//     while slowing down the logging routine with one more test to perform.
//   - Fast repeating log messages could trash the filesystem badly, slowing
//     down everything whenever they happened.
//   - The *_once and *_ONCE macros were not logging just once, but rather
//     sparsingly (the first time, then the 10th, the 100th and finally every
//     1000 occurrences), which forced to recourse to static booleans and tests
//     in place where you wanted to really only log a message once and for all
//     during the whole session.
//   - There was no way to get precise time stamps to measure the logged events
//     occurrence with a better precision than 1 second.
//   - The code was poorly optimized.
//
// I therefore:
//   - Kept only llinfos, llwarns, llerrs and LL_DEBUGS("Tag").
//   - Added a way to enable/disable LL_DEBUGS from a floater ("Advanced"
//     -> "Consoles" -> "Debug tags"; see also the newview/hbfloaterdebugtags.*
//     files); this also sets the sDebugMessages boolean accordingly (see the
//     optimizations point below).
//   - Got rid of broad/narrow tags (dual tags).
//   - Added llinfos_once and llwarns_once which replace LL_INFOS_ONCE("Tag")
//     and LL_WARNS_ONCE("Tag"), respectively, but kept LL_DEBUGS_ONCE("Tag").
//   - Made all *_once and LL_DEBUGS_ONCE macros indeed only logging once and
//     for all, but added *_sparse and LL_DEBUGS_SPARSE macros for when
//     occasionnal logging of often repeating cases is desired (i.e. *_sparse
//     and LL_DEBUGS_SPARSE act like LL's *_ONCE macros did).
//   - Added a repeating messages filter: messages repeated in succession are
//     now counted and only printed twice: the first time they occur, then a
//     second time, with the repeats count appended, whenever another,
//     different message is finally logged.
//   - Added an option to log timestamps with milliseconds.
//   - Brought many optimizations with, for example, faster tags lookups,
//     replacement of the glogal mUniqueLogMessages map (which was using
//     strings as keys, i.e. super-slow to search, and this caused _ONCE sites
//     to be skipped should they share the exact same message with another,
//     impairing the detection of bugs by hiding its location in the code) with
//     a per-site message hash / counter map, the use of the sDebugMessages
//     boolean to avoid entering a LL_DEGUGS* call site code when no debug tag
//     is activated at all, etc...

///////////////////////////////////////////////////////////////////////////////
// Error Logging Facility
//
//	Information for most users:
//
//	Code can log messages with constructions like this:
//
//		llinfos << "Request to fizzbip agent " << agent_id
//				<< " denied due to timeout" << llendl;
//
//	Messages can be logged to one of four increasing levels of concern, using
//	one of four "streams":
//
//		LL_DEBUGS("Tag")	- debug messages that are not shown unless "Tag"
//							  is active.
//		llinfos				- informational messages.
//		llwarns				- warning messages that signal an unexpected
//							  occurrence (that could be or not the sign of an
//							  actual problem).
//		llerrs				- error messages that are major, unrecoverable
//							  failures.
//	The later (llerrs) automatically crashes the process after the message is
//	logged.
//
//	Note that these "streams" are actually #define magic. Rules for use:
//		* they cannot be used as normal streams, only to start a message;
//		* messages written to them MUST be terminated with llendl or LL_ENDL;
//		* between the opening and closing, the << operator is indeed
//		  writing onto a std::ostream, so all conversions and stream
//		  formating are available.
//
//	These messages are automatically logged with function name, and (if
//	enabled) file and line of the message (note: existing messages that already
//	include the function name do not get name printed twice).
//
//	If you have a class, adding LOG_CLASS line to the declaration will cause
//	all messages emitted from member functions (normal and static) to be tagged
//	with the proper class name as well as the function name:
//
//		class LLFoo
//		{
//		protected:
//			LOG_CLASS(LLFoo);
//
//		public:
//			...
//		};
//
//		void LLFoo::doSomething(int i)
//		{
//			if (i > 100)
//			{
//				llwarns << "called with a big value for i: " << i << llendl;
//			}
//			...
//		}
//
//	will result in messages like:
//
//		WARNING: LLFoo::doSomething: called with a big value for i: 283
//
//
//	You may also use this construct if you need to do computation in the middle
//	of a message (most useful with debug messages):
//		LL_DEBUGS ("AgentGesture") << "the agent " << agend_id;
//		switch (f)
//		{
//			case FOP_SHRUGS:	LL_CONT << "shrugs";			break;
//			case FOP_TAPS:		LL_CONT << "points at " << who;	break;
//			case FOP_SAYS:		LL_CONT << "says " << message;	break;
//		}
//		LL_CONT << " for " << t << " seconds" << LL_ENDL;
//
//	Such computation is done only when the message is actually logged.
//
//
//	Which messages are logged and which are suppressed can be controled at run
//	time from the live file logcontrol.xml based on function, class and/or
//	source file.
//
//	Lastly, logging is now very efficient in both compiled code and execution
//	when skipped. There is no need to wrap messages, even debugging ones.
///////////////////////////////////////////////////////////////////////////////

namespace LLError
{
	enum ELevel
	{
		LEVEL_DEBUG = 0,
		LEVEL_INFO = 1,
		LEVEL_WARN = 2,
		LEVEL_ERROR = 3,
		// Not really a level: used to indicate that no messages should be
		// logged.
		LEVEL_NONE = 4
	};

	// Macro support
	// The classes CallSite and Log are used by the logging macros below.
	// They are not intended for general use.

	class CallSite;

	// Purely static class
	class Log
	{
	public:
		Log() = delete;
		~Log() = delete;

		static bool shouldLog(CallSite& site);
		static std::ostringstream* out();
		static void flush(const std::ostringstream& out, const CallSite& site);

	public:
		// When false, skip all LL_DEBUGS checks, for speed.
		static bool sDebugMessages;
		// When true, print milliseconds in timestamp for log messages.
		static bool sPreciseTimeStamp;
		// When true, abort() on llerrs instead of crashing.
		static bool sIsBeingDebugged;
	};

	class CallSite
	{
		friend class Log;

	public:
		// Represents a specific place in the code where a message is logged
		// This is public because it is used by the macros below. It is not
		// intended for public use. The constructor is never inlined since it
		// is called only once per static call site in the code and inlining it
		// would just consume needlessly CPU instruction cache lines... HB
		LL_NO_INLINE CallSite(ELevel level, const char* file, S32 line,
							  const std::type_info& class_info,
							  const char* function, const char* tag);

		virtual ~CallSite() = default;

		// This member function needs to be in-line for efficiency
		LL_INLINE bool shouldLog()
		{
			return mCached ? mShouldLog : Log::shouldLog(*this);
		}

		LL_INLINE void invalidate()						{ mCached = false; }

		// This method is for adding any prefix to the log message. For this
		// base class, nothing is added. When it returns true, the message
		// is logged by the caller, else the log line is discarded. HB
		LL_INLINE virtual bool getPrefix(std::ostringstream& out,
										 const std::string& message) const
		{
			return true;
		}

	private:
		// These describe the call site and never change
		const ELevel			mLevel;
		const S32				mLine;
		char*					mFile;
		const std::type_info&   mClassInfo;
		const char* const		mFunction;
		const char* const		mTag;

		// These implement a cache of the call to shouldLog()
		bool					mCached;
		bool					mShouldLog;
	};

	// We use a derived method to avoid storing a hash map and a superfluous
	// boolean for call sites which are not of the ONCE or SPARSE types. HB
	class CallSiteOnce final : public CallSite
	{
		friend class Log;

	public:
		// Represents a specific place in the code where a message is logged
		// This is public because it is used by the macros below. It is not
		// intended for public use. The constructor is never inlined since it
		// is called only once per static call site in the code and inlining it
		// would just consume needlessly CPU instruction cache lines... HB
		LL_NO_INLINE CallSiteOnce(ELevel level, const char* file, S32 line,
								  const std::type_info& class_info,
								  const char* function, const char* tag,
								  bool sparse);

		// This method allows to decide whether to log or not based on the past
		// occurrences of 'message', and sends the corresponding prefix to 'out'
		// when logging should happen, returning true. It returns false if the
		// 'message' has already be seen and the log line must be discarded. HB
		LL_NO_INLINE bool getPrefix(std::ostringstream& out,
									const std::string& message) const override;

	private:
		// This stores the hashes of the messages already printed for this call
		// site. HB
		typedef flat_hmap<U64, U32> msg_hash_map_t;
		// 'mutable', because we need the getPrefix() method to be const while
		// modifying this map. HB
		mutable msg_hash_map_t	mOccurrences;
		// true = sparse messages, false = print them only once.
		const bool				mSparse;
	};

	// Used to indicate the end of a message
	class End {};
	LL_INLINE std::ostream& operator<<(std::ostream& s, const End&)
	{
		return s;
	}

	// Used to indicate no class info known for logging
	class NoClassInfo {};

	std::string className(const std::type_info& type);

	bool isAvailable();
}

#if defined(LL_DEBUG)
# define llassert(func)			llassert_always(func)
# define LL_HAS_ASSERT 1
#elif defined(LL_LOOP_ON_ASSERT)
// Useful for debugging optimized code when it takes unsuspected paths and gdb
// fails to trace it back. HB
# define llassert(func)			if (LL_UNLIKELY(!(func))) { while (true) ; }
# define LL_HAS_ASSERT 1
#else
# define llassert(func)
#endif
#define llassert_always(func)	if (LL_UNLIKELY(!(func))) llerrs << "ASSERT (" << #func << ")" << llendl;

//
// Class type information for logging
//

// Declares class to tag logged messages with. See top of file for example of
// how to use this
#define LOG_CLASS(s)		typedef s _LL_CLASS_TO_LOG

// Outside a class declaration, or in class without LOG_CLASS(), this typedef
// causes the messages to not be associated with any class.
typedef LLError::NoClassInfo _LL_CLASS_TO_LOG;

//
// Error Logging Macros. See top of file for common usage.
//

#define lllog(level) \
	{ \
		static LLError::CallSite _site(level, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, NULL); \
		if (LL_LIKELY(_site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define lllog2(level, sparse) \
	{ \
		static LLError::CallSiteOnce _site(level, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, NULL, sparse); \
		if (LL_LIKELY(_site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define llendl \
			LLError::End(); \
			LLError::Log::flush(_out, _site); \
		} \
	}

#define llinfos					lllog(LLError::LEVEL_INFO)
#define llinfos_once			lllog2(LLError::LEVEL_INFO, false)
#define llinfos_sparse			lllog2(LLError::LEVEL_INFO, true)
#define llwarns					lllog(LLError::LEVEL_WARN)
#define llwarns_once			lllog2(LLError::LEVEL_WARN, false)
#define llwarns_sparse			lllog2(LLError::LEVEL_WARN, true)
#define llerrs					lllog(LLError::LEVEL_ERROR)
#define llcont					(_out)

// Macros for debugging with the passing of a string tag. Note that we test for
// a special static variable (sDebugMessages) before calling shouldLog(), which
// allows to switch off all debug messages logging at once if/when needed, and
// speeds up tremendously the code when no debug tag is activated by avoiding a
// pointless call to shouldLog(). HB

#define lllog_debug(Tag) \
	{ \
		static LLError::CallSite _site(LLError::LEVEL_DEBUG, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, Tag); \
		if (LL_UNLIKELY(LLError::Log::sDebugMessages && _site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define lllog_debug2(Tag, sparse) \
	{ \
		static LLError::CallSiteOnce _site(LLError::LEVEL_DEBUG, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, Tag, sparse); \
		if (LL_UNLIKELY(LLError::Log::sDebugMessages && _site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define LL_DEBUGS(Tag)			lllog_debug(Tag)
#define LL_DEBUGS_ONCE(Tag)		lllog_debug2(Tag, false)
#define LL_DEBUGS_SPARSE(Tag)	lllog_debug2(Tag, true)
#define LL_ENDL					llendl
#define LL_CONT					(_out)

#endif // LL_LLERROR_H
