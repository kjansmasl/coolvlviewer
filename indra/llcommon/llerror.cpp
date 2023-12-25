/**
 * @file llerror.cpp
 * @brief Error message system implementation
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

#include "linden_common.h"

#include "llerrorcontrol.h"

#include <cctype>
#include <chrono>
#include <sstream>
#if LL_WINDOWS
# include <windows.h>
# include <io.h>
#else // LL_WINDOWS
# include <cxxabi.h>
# include <stdlib.h>	// getenv()
# include <syslog.h>
# include <unistd.h>
#endif // !LL_WINDOWS
#if LL_DARWIN
# include <sys/stat.h>
#endif

#include "llapp.h"
#include "hbfastmap.h"
#include "lllivefile.h"
#include "llmutex.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsingleton.h"
#include "llstl.h"
#include "llstring.h"
#include "lltimer.h"
#include "hbxxh.h"

bool LLError::Log::sDebugMessages = true;
bool LLError::Log::sPreciseTimeStamp = false;
bool LLError::Log::sIsBeingDebugged = false;

namespace
{
#if LL_WINDOWS
	// Be careful when calling OutputDebugString as it throws
	// DBG_PRINTEXCEPTION_C which works just fine under the windows debugger,
	// but can cause users who have enabled SEHOP exception chain validation to
	// crash due to interactions between the Win 32-bit exception handling and
	// boost coroutine fiber stacks. BUG-2707
	LL_INLINE void LLOutputDebugUTF8(const std::string& s)
	{
		// Need UTF16 for Unicode OutputDebugString
		if (IsDebuggerPresent() && s.size())
		{
			OutputDebugString(ll_convert_string_to_wide(s).c_str());
			OutputDebugString(TEXT("\n"));
		}
	}

	class RecordToWinDebug final : public LLError::Recorder
	{
	public:
		void recordMessage(LLError::ELevel, const std::string& msg) override
		{
			LLOutputDebugUTF8(msg);
		}
	};
#else
	class RecordToSyslog final : public LLError::Recorder
	{
	public:
		RecordToSyslog(const std::string& identity)
		:	mIdentity(identity)
		{
			openlog(mIdentity.c_str(), LOG_CONS|LOG_PID, LOG_LOCAL0);
				// we need to set the string from a local copy of the string
				// since apparanetly openlog expects the const char* to remain
				// valid even after it returns (presumably until closelog)
		}

		~RecordToSyslog()
		{
			closelog();
		}

		void recordMessage(LLError::ELevel lv, const std::string& msg) override
		{
			int prio = LOG_CRIT;
			switch (lv)
			{
				case LLError::LEVEL_DEBUG:	prio = LOG_DEBUG;	break;
				case LLError::LEVEL_INFO:	prio = LOG_INFO;	break;
				case LLError::LEVEL_WARN:	prio = LOG_WARNING; break;
				case LLError::LEVEL_ERROR:	prio = LOG_CRIT;	break;
				default:					prio = LOG_CRIT;
			}

			syslog(prio, "%s", msg.c_str());
		}

	private:
		std::string mIdentity;
	};
#endif

	class RecordToFile final : public LLError::Recorder
	{
	public:
		RecordToFile(const std::string& filename)
		:	mName(filename),
#if LL_LINUX || LL_DARWIN
			mSavedStderr(-1),
#endif
			mLogFile(LLFile::open(filename, "a"))
		{
			if (!mLogFile)
			{
				llwarns << "Error setting log file to " << filename << llendl;
			}
#if LL_LINUX || LL_DARWIN
			else if (getenv("LL_REDIRECT_STDERR_TO_LOG"))
			{
				// We use a number of classic-C libraries, some of which write
				// log output to stderr. The trouble with that is that unless
				// you launch the viewer from a console, stderr output is lost.
				// Redirect STDERR_FILENO to write into this log file. But
				// first, save the original stream in case we want it later.
				mSavedStderr = dup(STDERR_FILENO);
				dup2(fileno(mLogFile), STDERR_FILENO);
			}
#endif
		}

		~RecordToFile() override
		{
#if LL_LINUX || LL_DARWIN
			if (mSavedStderr >= 0)
			{
				// Restore stderr to its original fileno so any subsequent
				// output to stderr goes to original stream.
				dup2(mSavedStderr, STDERR_FILENO);
			}
#endif
			mLogFile.flush();	// Paranoia
		}

		LL_INLINE bool okay()					{ return bool(mLogFile); }

		LL_INLINE bool wantsTime() override		{ return true; }

		LL_INLINE void recordMessage(LLError::ELevel,
									 const std::string& msg) override
		{
			fwrite(msg.c_str(), sizeof(char), msg.length(), mLogFile);
			putc('\n', mLogFile);
			flushIfNeeded();
		}

		// This method flushes to the disk only when needed, so to keep the
		// number of writes low enough (especially important with SSDs and
		// their limited write endurance). HB
		LL_NO_INLINE void flushIfNeeded()
		{
			static LLTimer flush_timer;
			// In the Cool VL Viewer, sDebugMessages is true on startup and,
			// once logged in, when any LL_DEBUGS tag is active; we want to
			// see log messages in real time in the latter case, so let's
			// always flush them.
			bool do_flush = LLError::Log::sDebugMessages ||
#if LL_LINUX || LL_DARWIN
			// We also need to always flush whenever the stderr stream is
			// redirected to our log stream, else race conditions will happen
			// between viewer log messages and third parties libraries/binaries
			// log messages, causing mangled log lines.
							mSavedStderr >= 0 ||
#endif
			// Flush at least once every 10 seconds otherwise.
							flush_timer.getElapsedTimeF64() >= 10.0;
			if (do_flush)
			{
				mLogFile.flush();
				flush_timer.reset();
			}
		}

		LL_INLINE const std::string& getFilename() const
		{
			return mName;
		}

	private:
		LLFile			mLogFile;
		std::string		mName;
#if LL_LINUX || LL_DARWIN
		int				mSavedStderr;
#endif
	};

	class RecordToStderr final : public LLError::Recorder
	{
	public:
		RecordToStderr()
		:	mUseANSI(ANSI_PROBE)
		{
		}

#if LL_WINDOWS
		LL_INLINE bool wantsTime() override		{ return false; }
#else
		LL_INLINE bool wantsTime() override		{ return true; }
#endif

		void recordMessage(LLError::ELevel lv, const std::string& msg) override
		{
			// Default all message levels to bold so we can distinguish our
			// own messages from those dumped by subprocesses and libraries.
			static std::string s_ansi_bold = createANSI("1");	// Bold
			static std::string s_ansi_error = s_ansi_bold +
											  createANSI("31");	// Red
			static std::string s_ansi_warn = s_ansi_bold +
											 createANSI("34");	// Blue
			static std::string s_ansi_debug = s_ansi_bold +
											  createANSI("35");	// Magenta
			if (ANSI_PROBE == mUseANSI)
			{
				mUseANSI = checkANSI() ? ANSI_YES : ANSI_NO;
			}

			if (ANSI_YES != mUseANSI)
			{
				fprintf(stderr, "%s\n", msg.c_str());
				return;
			}

			switch (lv)
			{
				case LLError::LEVEL_ERROR:
					writeANSI(s_ansi_error, msg);
					return;

				case LLError::LEVEL_WARN:
					writeANSI(s_ansi_warn, msg);
					return;

				case LLError::LEVEL_DEBUG:
					writeANSI(s_ansi_debug, msg);
					return;

				default:
					writeANSI(s_ansi_bold, msg);
			}
		}

	private:
		LL_INLINE std::string createANSI(const std::string& code)
		{
			return "\033[" + code + "m";
		}

		LL_INLINE void writeANSI(const std::string& ansi_code,
								 const std::string& message)
		{
			static std::string s_ansi_reset = createANSI("0");
			fprintf(stderr, "%s%s%s\n", ansi_code.c_str(), message.c_str(),
					s_ansi_reset.c_str());
		}

		LL_INLINE bool checkANSI()
		{
			// Check whether it is okay to use ANSI; if stderr is a TTY then we
			// assume yes. Can be turned off with the LL_NO_ANSI_COLOR env var.
			return isatty(2) != 0 && getenv("LL_NO_ANSI_COLOR") == NULL;
		}

	private:
		enum ANSIState { ANSI_PROBE, ANSI_YES, ANSI_NO };
		ANSIState	mUseANSI;
	};

	class RecordToFixedBuffer final : public LLError::Recorder
	{
	public:
		RecordToFixedBuffer(LLLineBuffer* buffer)
		:	mBuffer(buffer)
		{
		}

		void recordMessage(LLError::ELevel, const std::string& msg) override
		{
			mBuffer->addLine(msg);
		}

	private:
		LLLineBuffer* mBuffer;
	};

	class LogControlFile final : public LLLiveFile
	{
	protected:
		LOG_CLASS(LogControlFile);

	public:
		static LogControlFile& fromDirectory(const std::string& dir);

		bool loadFile() override;

	private:
		LogControlFile(const std::string& filename)
		:	LLLiveFile(filename)
		{
		}
	};

	LogControlFile& LogControlFile::fromDirectory(const std::string& dir)
	{
#if LL_WINDOWS
		std::string file = dir + "\\logcontrol.xml";
#else
		std::string file = dir + "/logcontrol.xml";
#endif
		return *new LogControlFile(file);	// NB: this instance is never freed
	}

	bool LogControlFile::loadFile()
	{
		LLSD configuration;

		llifstream file(filename().c_str());
		if (file.is_open())
		{
			LLSDSerialize::fromXML(configuration, file);
		}

		if (configuration.isUndefined())
		{
			llwarns << filename()
					<< " missing, ill-formed or simply undefined; not changing configuration."
					<< llendl;
			return false;
		}

		LLError::configure(configuration);
		llinfos << "logging reconfigured from " << filename() << llendl;
		return true;
	}

	typedef flat_hmap<std::string, LLError::ELevel> level_map_t;
	typedef std::vector<LLError::Recorder*> rec_list_t;
	typedef std::vector<LLError::CallSite*> callsite_vect_t;
	typedef flat_hmap<std::string, U32> uniq_msg_map_t;

	class Globals : public LLSingleton<Globals>
	{
		friend class LLSingleton<Globals>;

	public:
		Globals() = default;

		void addCallSite(LLError::CallSite& site)
		{
			mCallSites.push_back(&site);
		}

		void invalidateCallSites()
		{
			for (callsite_vect_t::const_iterator it = mCallSites.begin(),
												 end = mCallSites.end();
				 it != end; ++it)
			{
				(*it)->invalidate();
			}

			mCallSites.clear();
		}

	private:
		callsite_vect_t		mCallSites;
	};
}

namespace LLError
{
	class Settings
	{
	private:
		Settings()
		:	mPrintLocation(false),
			mDefaultLevel(LLError::LEVEL_DEBUG),
			mCrashFunction(NULL),
			mTimeFunction(NULL),
			mFileRecorder(NULL),
			mFixedBufferRecorder(NULL)
		{
		}

		~Settings()
		{
			for_each(mRecorders.begin(), mRecorders.end(), DeletePointer());
			mRecorders.clear();
		}

	public:
		static Settings*& getPtr();
		static Settings& get();

		static void reset();
		static Settings* saveAndReset();
		static void restore(Settings*);

	public:
		LLError::ELevel			mDefaultLevel;

		level_map_t				mFunctionLevelMap;
		level_map_t				mClassLevelMap;
		level_map_t				mFileLevelMap;
		level_map_t				mTagLevelMap;

		LLError::fatal_func_t	mCrashFunction;
		LLError::time_func_t	mTimeFunction;

		rec_list_t				mRecorders;
		Recorder*				mFileRecorder;
		Recorder*				mFixedBufferRecorder;
		std::string				mFileRecorderFileName;

		bool					mPrintLocation;
	};

	LL_INLINE Settings& Settings::get()
	{
		Settings* p = getPtr();
		if (!p)
		{
			reset();
			p = getPtr();
		}
		return *p;
	}

	void Settings::reset()
	{
		Globals::getInstance()->invalidateCallSites();

		Settings*& p = getPtr();
		delete p;
		p = new Settings();
	}

	Settings* Settings::saveAndReset()
	{
		Globals::getInstance()->invalidateCallSites();

		Settings*& p = getPtr();
		Settings* originalSettings = p;
		p = new Settings();
		return originalSettings;
	}

	void Settings::restore(Settings* originalSettings)
	{
		Globals::getInstance()->invalidateCallSites();

		Settings*& p = getPtr();
		delete p;
		p = originalSettings;
	}

	LL_INLINE Settings*& Settings::getPtr()
	{
		static Settings* currentSettings = NULL;
		return currentSettings;
	}

	bool isAvailable()
	{
		return Settings::getPtr() != NULL && Globals::instanceExists();
	}

	LL_INLINE void removePrefix(char* s, const char* p)
	{
		char* where = strstr(s, p);
		if (where)
		{
			s = where + strlen(p);
		}
	}

	LL_NO_INLINE CallSite::CallSite(ELevel level, const char* file, S32 line,
								    const std::type_info& class_info,
								    const char* function, const char* tag)
	:	mLevel(level),
		mLine(line),
		mClassInfo(class_info),
		mFunction(function),
		mCached(false),
		mShouldLog(false),
		mTag(tag)
	{
		// This indeed points to a C-string constant, but we change the start
		// of the string to remove useless verbosity; done here once and for
		// all to avoid calling removePrefix() in time critical sections. HB
		mFile = (char*)file;
#if LL_WINDOWS
		removePrefix(mFile, "indra\\");
#else
		removePrefix(mFile, "indra/");
#endif
#if LL_DARWIN
		removePrefix(mFile, "newview/../");
#endif
	}

	LL_NO_INLINE CallSiteOnce::CallSiteOnce(ELevel level, const char* file, S32 line,
										    const std::type_info& class_info,
										    const char* function, const char* tag,
											bool sparse)
	:	CallSite(level, file, line, class_info, function, tag),
		mSparse(sparse)
	{
	}

	//virtual
	LL_NO_INLINE bool CallSiteOnce::getPrefix(std::ostringstream& out,
											  const std::string& msg) const
	{
		// Using a (fast !) hash as a key for the map (instead of the 'msg'
		// string, originally) saves memory and makes searches in the map much
		// faster. HB
		U64 hash = HBXXH64::digest(msg);
		msg_hash_map_t::iterator it = mOccurrences.find(hash);
		if (it == mOccurrences.end())
		{
			mOccurrences[hash] = 1;
			out << (mSparse ? "SPARSE: " : "ONCE: ");
			return true;
		}
		if (mSparse)
		{
			U32 num_messages = ++it->second;
			if (num_messages == 10 || num_messages == 100 ||
				num_messages == 1000 || num_messages % 10000 == 0)
			{
				out << "SPARSE (" << num_messages << "th time seen): ";
				return true;
			}
		}
		return false;
	}

#if LL_DARWIN
	bool shouldLogToStderr()
	{
		if (getenv("LL_REDIRECT_STDERR_TO_LOG"))
		{
			// Do not log to stderr when we redirect the latter to the log file
			// (else we get duplicates for every viewer log messages).
			return false;
		}

		// On Mac OS X, stderr from apps launched from the Finder goes to the
		// console log. It is generally considered bad form to spam too much
		// there. That scenario can be detected by noticing that stderr is a
		// character device (S_IFCHR). If stderr is a TTY or a pipe, assume the
		// user launched from the command line or debugger and therefore wants
		// to see stderr.
		if (isatty(STDERR_FILENO))
		{
			return true;
		}

		struct stat st;
		if (fstat(STDERR_FILENO, &st) >= 0)
		{
			// fstat() worked: allow to log only if stderr is a pipe
			return (st.st_mode & S_IFMT) == S_IFIFO;
		}

		// Got called during log-system setup: cannot fstat() just yet, so just
		// report the issue and give-up logging...
		int errno_save = errno;
		std::cerr << "shouldLogToStderr: fstat(" << STDERR_FILENO
				  << ") failed with errno: " << errno_save << std::endl;
		return false;
	}
#elif LL_LINUX
	bool shouldLogToStderr()
	{
		// Do not log to stderr when we redirect the latter to the log file
		// (else we get duplicates for every viewer log messages).
		return !getenv("LL_REDIRECT_STDERR_TO_LOG");
	}
#endif

	void commonInit(const std::string& dir)
	{
		Settings::reset();

		setDefaultLevel(LEVEL_INFO);
		setTimeFunction(utcTime);

#if LL_WINDOWS
		addRecorder(new RecordToStderr);
		addRecorder(new RecordToWinDebug);
#else
		if (shouldLogToStderr())
		{
			addRecorder(new RecordToStderr);
		}
#endif

		LogControlFile& e = LogControlFile::fromDirectory(dir);

		// NOTE: We want to explicitly load the file before we add it to the
		// event timer that checks for changes to the file. Else, we are not
		// actually loading the file yet and most of the initialization happens
		// without any attention being paid to the log control file. Not to
		// mention that when it finally gets checked later, all log statements
		// that have been evaluated already become dirty and need to be
		// evaluated for printing again. So, make sure to call checkAndReload()
		// before addToEventTimer().
		e.checkAndReload();
		e.addToEventTimer();
	}

	void initForApplication(const std::string& dir)
	{
		commonInit(dir);
	}

	void setPrintLocation(bool print)
	{
		Settings& s = Settings::get();
		s.mPrintLocation = print;
	}

	void setFatalFunction(fatal_func_t f)
	{
		Settings& s = Settings::get();
		s.mCrashFunction = f;
	}

	void setTimeFunction(time_func_t f)
	{
		Settings& s = Settings::get();
		s.mTimeFunction = f;
	}

	void setDefaultLevel(ELevel level)
	{
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		s.mDefaultLevel = level;
	}

	void setFunctionLevel(const std::string& function_name, ELevel level)
	{
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		s.mFunctionLevelMap.emplace(function_name, level);
	}

	void setClassLevel(const std::string& class_name, ELevel level)
	{
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		s.mClassLevelMap.emplace(class_name, level);
	}

	void setFileLevel(const std::string& file_name, ELevel level)
	{
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		s.mFileLevelMap.emplace(file_name, level);
	}

	void setTagLevel(const std::string& tag_name, ELevel level)
	{
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		s.mTagLevelMap.emplace(tag_name, level);
	}

	ELevel getTagLevel(const std::string& tag_name)
	{
		Settings& s = Settings::get();
		return s.mTagLevelMap[tag_name];
	}

	std::set<std::string> getTagsForLevel(ELevel level)
	{
		std::set<std::string> tags;
		Globals::getInstance()->invalidateCallSites();
		Settings& s = Settings::get();
		for (level_map_t::const_iterator it = s.mTagLevelMap.begin(),
										 end = s.mTagLevelMap.end();
			 it != end; ++it)
		{
			if (it->second == level)
			{
				tags.emplace(it->first);
			}
		}
		return tags;
	}

	ELevel decodeLevel(std::string name)
	{
		static level_map_t level_names;
		if (level_names.empty())
		{
			level_names["ALL"] = LEVEL_DEBUG;
			level_names["DEBUG"] = LEVEL_DEBUG;
			level_names["INFO"] = LEVEL_INFO;
			level_names["WARN"] = LEVEL_WARN;
			level_names["ERROR"] = LEVEL_ERROR;
			level_names["NONE"] = LEVEL_NONE;
		}

		LLStringUtil::toUpper(name);
		level_map_t::const_iterator it = level_names.find(name);
		if (it == level_names.end())
		{
			llwarns << "Unrecognized logging level: '" << name << "'"
					<< llendl;
			return LEVEL_INFO;
		}
		return it->second;
	}

	void setLevels(level_map_t& map, const LLSD& list, ELevel level)
	{
		for (LLSD::array_const_iterator it = list.beginArray(),
										end = list.endArray();
			 it != end; ++it)
		{
			map.emplace(it->asString(), level);
		}
	}

	void configure(const LLSD& config)
	{
		Globals::getInstance()->invalidateCallSites();

		Settings& s = Settings::get();
		s.mFunctionLevelMap.clear();
		s.mClassLevelMap.clear();
		s.mFileLevelMap.clear();
		s.mTagLevelMap.clear();

		setPrintLocation(config["print-location"]);
		setDefaultLevel(decodeLevel(config["default-level"]));

		LLSD sets = config["settings"];
		for (LLSD::array_const_iterator it = sets.beginArray(),
										end = sets.endArray();
			 it != end; ++it)
		{
			const LLSD& entry = *it;
			ELevel level = decodeLevel(entry["level"]);
			setLevels(s.mFunctionLevelMap, entry["functions"], level);
			setLevels(s.mClassLevelMap, entry["classes"], level);
			setLevels(s.mFileLevelMap, entry["files"], level);
			setLevels(s.mTagLevelMap, entry["tags"], level);
		}
	}

	LL_INLINE Recorder::~Recorder() = default;

	LL_INLINE bool Recorder::wantsTime() 				{ return false; }

	void addRecorder(Recorder* recorder)
	{
		if (recorder)
		{
			Settings& s = Settings::get();
			s.mRecorders.push_back(recorder);
		}
	}

	void removeRecorder(Recorder* recorder)
	{
		if (!recorder)
		{
			return;
		}
		Settings& s = Settings::get();
		rec_list_t::iterator end = s.mRecorders.end();
		s.mRecorders.erase(std::remove(s.mRecorders.begin(), end, recorder),
						   end);
	}

	void logToFile(const std::string& file_name)
	{
		Settings& s = Settings::get();

		removeRecorder(s.mFileRecorder);
		delete s.mFileRecorder;
		s.mFileRecorder = NULL;
		s.mFileRecorderFileName.clear();

		if (file_name.empty())
		{
			return;
		}

		RecordToFile* f = new RecordToFile(file_name);
		if (!f->okay())
		{
			delete f;
			return;
		}

		s.mFileRecorderFileName = file_name;
		s.mFileRecorder = f;
		addRecorder(f);
	}

	void logToFixedBuffer(LLLineBuffer* fixed_bufp)
	{
		Settings& s = Settings::get();

		removeRecorder(s.mFixedBufferRecorder);
		delete s.mFixedBufferRecorder;
		s.mFixedBufferRecorder = NULL;

		if (!fixed_bufp)
		{
			return;
		}

		s.mFixedBufferRecorder = new RecordToFixedBuffer(fixed_bufp);
		addRecorder(s.mFixedBufferRecorder);
	}

	std::string logFileName()
	{
		Settings& s = Settings::get();
		return s.mFileRecorderFileName;
	}

	void setLogFileName(std::string filename)
	{
		Settings& s = Settings::get();
		s.mFileRecorderFileName = filename;
	}

	// Recorder formats:
	//
	// $type = "ERROR" | "WARNING" | "INFO" | "DEBUG"
	// $loc = "$file($line)"
	// $msg = "$loc : " if FATAL or printing loc, "" otherwise
	// $msg += "$type: "
	// $msg += contents of stringstream
	// $time = "%Y-%m-%d %H:%M:%SZ" (UTC)
	//
	// syslog:	"$msg"
	// file: "$time $msg\n"
	// stderr: "$time $msg\n" except on windows, "$msg\n"
	// fixedbuf: "$msg"
	// winddebug: "$msg\n"
	//
	// Note: if FATAL, an additional line gets logged first, with $msg set to
	//  	 "$loc : error"
	//
	// You get:
	//	llfoo.cpp(42) : error
	//	llfoo.cpp(42) : ERROR: something

	// This ensures the static mutex gets constructed on first use, which is
	// otherwise not the case with boost mutexes, resulting in a hang at
	// startup... HB
	LLMutex& getLogMutex()
	{
		static LLMutex mutex;
		return mutex;
	}

	LL_NO_INLINE std::string className(const std::type_info& type)
	{
		std::string name = type.name();
#if LL_DARWIN
		// libc++'s type_info::name() returns a mangled class name, must
		// demangle
		// Newest macOS versions got a crash bug in abi::__cxa_demangle()
		// when it is passed a static buffer pointer, so just let that
		// method allocate the memory by itself (slower than a static buffer,
		// of course)...
		int status;	// Not used by us but needed by __cxa_demangle()...
		char* c_str = abi::__cxa_demangle(name.c_str(), NULL, 0, &status);
		if (c_str)
		{
			name.assign(c_str);
			free((void*)c_str);
		}
#elif LL_LINUX
		// libstdc++'s type_info::name() returns a mangled class name, must
		// demangle
		static size_t abi_name_len = 1024;	// Large enough to avoid realloc...
		static char* abi_name_buf = (char*)malloc(abi_name_len);

		int status;	// Not used by us but needed by __cxa_demangle()...
		char* c_str = abi::__cxa_demangle(name.c_str(), abi_name_buf,
										  &abi_name_len, &status);
		if (c_str)
		{
			name.assign(c_str);
		}
#elif LL_WINDOWS
		// MSVC runtimes' type_info::name() includes the text "class " at the
		// start
		static const std::string class_prefix = "class ";
		if (name.compare(0, 6, class_prefix) == 0)
		{
			return name.substr(6);
		}
# if 0	// ... or "struct "... But we do not use logging macros in structures,
		// in the Cool VL Viewer, so we do not care !  HB
		static const std::string struct_prefix = "struct ";
		if (name.compare(0, 7, struct_prefix) == 0)
		{
			return name.substr(7);
		}
# endif
#endif
		return name;
	}

	LL_INLINE bool checkLevelMap(const level_map_t& map,
								 const std::string& key, ELevel& level)
	{
		level_map_t::const_iterator it = map.find(key);
		if (it == map.end())
		{
			return false;
		}
		level = it->second;
		return true;
	}

	LL_NO_INLINE bool Log::shouldLog(CallSite& site)
	{
		LLMutexLock lock(getLogMutex());

		Settings& s = Settings::get();

		ELevel level = s.mDefaultLevel;

		std::string class_name = className(site.mClassInfo);
		std::string function_name = site.mFunction;
		if (!s.mFunctionLevelMap.empty())
		{
#if LL_MSVC
			// DevStudio: the __FUNCTION__ macro string includes the type
			// and/or namespace prefixes... Remove them.
			size_t p = function_name.rfind(':');
			if (p != std::string::npos)
			{
				function_name = function_name.substr(p + 1);
			}
#endif
			static const std::string no_class_info = "LLError::NoClassInfo";
			if (class_name != no_class_info)
			{
				function_name = class_name + "::" + function_name;
			}
		}

		// The most specific match found will be used as the log level, since
		// the computation short circuits. So, in increasing order of
		// importance:
		// Default < Tag < File < Class < Function
		checkLevelMap(s.mFunctionLevelMap, function_name, level)
		|| checkLevelMap(s.mClassLevelMap, class_name, level)
		|| checkLevelMap(s.mFileLevelMap, site.mFile, level)
		|| (site.mTag && checkLevelMap(s.mTagLevelMap, site.mTag, level));

		site.mCached = true;
		Globals::getInstance()->addCallSite(site);
		return site.mShouldLog = site.mLevel >= level;
	}

	LL_NO_INLINE void writeToRecorders(ELevel level,
									   const std::string& message)
	{
		static std::string last_message;
		static U32 repeats = 0;

		if (message == last_message)
		{
			++repeats;
			return;
		}

		if (repeats > 1)
		{
			last_message += llformat(" (repeated %d times)", repeats);
		}

		Settings& s = Settings::get();
		if (s.mTimeFunction)
		{
			std::string message_with_time =
				s.mTimeFunction(Log::sPreciseTimeStamp) + " ";
			std::string last_with_time;
			if (repeats > 0)
			{
				last_with_time = message_with_time + last_message;
			}
			message_with_time += message;

			for (rec_list_t::const_iterator it = s.mRecorders.begin(),
											end = s.mRecorders.end();
				 it != end; ++it)
			{
				Recorder* r = *it;
				if (r->wantsTime())
				{
					if (repeats > 0)
					{
						r->recordMessage(level, last_with_time);
					}
					r->recordMessage(level, message_with_time);
				}
				else
				{
					if (repeats > 0)
					{
						r->recordMessage(level, last_message);
					}
					r->recordMessage(level, message);
				}
			}
		}
		else
		{
			for (rec_list_t::const_iterator it = s.mRecorders.begin(),
											end = s.mRecorders.end();
				 it != end; ++it)
			{
				Recorder* r = *it;
				if (repeats > 0)
				{
					r->recordMessage(level, last_message);
				}
				r->recordMessage(level, message);
			}
		}

		repeats = 0;
		last_message = message;
	}

	LL_NO_INLINE void Log::flush(const std::ostringstream& out,
								 const CallSite& site)
	{
		LLMutexLock lock(getLogMutex());

		std::string message = out.str();

		if (site.mLevel == LEVEL_ERROR)
		{
			std::ostringstream fatal_msg;
			fatal_msg << site.mFile << "(" << site.mLine << ") : error";

			writeToRecorders(site.mLevel, fatal_msg.str());
		}

		std::ostringstream prefix;

		switch (site.mLevel)
		{
			case LEVEL_DEBUG:	prefix << "DEBUG: ";	break;
			case LEVEL_INFO:	prefix << "INFO: ";		break;
			case LEVEL_WARN:	prefix << "WARNING: ";	break;
			case LEVEL_ERROR:	prefix << "ERROR: ";	break;
			default:			prefix << "XXX: ";
		}

		Settings& s = Settings::get();
		if (s.mPrintLocation)
		{
			prefix << site.mFile << "(" << site.mLine << ") : ";
		}

#if !LL_MSVC	// DevStudio: __FUNCTION__ already includes the full class name
		std::string class_name = className(site.mClassInfo);
		static const std::string no_class_info = "LLError::NoClassInfo";
		if (class_name != no_class_info)
		{
			static const std::string anonymous = "(anonymous namespace)::";
			if (class_name.compare(0, 23, anonymous) == 0)
			{
				class_name = class_name.substr(23);
			}
			prefix << class_name << "::";
		}
#endif

		prefix << site.mFunction << ": ";

		// Possible ONCE and SPARSE message prefixes. When 'false' is returned,
		// the log line must be discarded. HB
		if (!site.getPrefix(prefix, message))
		{
			return;
		}

		prefix << message;
		message = prefix.str();

		writeToRecorders(site.mLevel, message);

		if (site.mLevel == LEVEL_ERROR)
		{
			// Do not call the crash function while being debugged, to avoid
			// polluting the stack trace with that function call. HB
			if (s.mCrashFunction && !sIsBeingDebugged)
			{
				s.mCrashFunction(message);
			}
			else
			{
				LL_ERROR_CRASH;
			}
		}
	}

	Settings* saveAndResetSettings()
	{
		return Settings::saveAndReset();
	}

	void restoreSettings(Settings* s)
	{
		return Settings::restore(s);
	}

	void replaceChar(std::string& s, char old, char replacement)
	{
		for (size_t i = 0, len = s.length(); i < len; ++i)
		{
			if (s[i] == old)
			{
				s[i] = replacement;
			}
		}
	}

	LL_NO_INLINE std::string utcTime(bool print_ms)
	{
		// We cache the last timestamp string and return it when this function
		// is called again soon enough for that string to stay unchanged. HB
		static bool last_print_ms = false;
		static time_t last_time = 0;
		static S32 last_ms = 0;
		static char time_str[64];

		if (print_ms)
		{
			auto sysclock = std::chrono::system_clock::now();
			time_t now = std::chrono::system_clock::to_time_t(sysclock);
			static const auto chrono_ms = std::chrono::milliseconds(1);
			S32 ms = (sysclock.time_since_epoch() / chrono_ms) % 1000;
			if (ms != last_ms || now != last_time || last_print_ms != print_ms)
			{
				last_print_ms = print_ms;
				last_time = now;
				last_ms = ms;
				strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", gmtime(&now));
				strcat(time_str, llformat(".%03dZ", ms).c_str());
			}
		}
		else
		{
			time_t now = time(NULL);
			if (now != last_time || last_print_ms != print_ms)
			{
				last_print_ms = print_ms;
				last_time = now;
				time_str[0] = '\0';
				strftime(time_str, 64, "%Y-%m-%d %H:%M:%SZ", gmtime(&now));
			}
		}

		return time_str;
	}
}
