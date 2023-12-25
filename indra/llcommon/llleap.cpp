/**
 * @file llleap.h
 * @brief Class implementation for LLeap, and its ancillary classes (LLProcess,
 *        LLProcessListener, LLLeapListener).
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010-2022, Linden Research, Inc.
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
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

#include "apr_thread_proc.h"
#include "apr_signal.h"

#include "boost/asio/streambuf.hpp"
#include "boost/asio/buffers_iterator.hpp"
#include "boost/ptr_container/ptr_vector.hpp"
#include "boost/scoped_ptr.hpp"

#include "llleap.h"

#include "llapr.h"
#include "lleventdispatcher.h"
#include "llsd.h"
#include "llsdutil.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lltimer.h"

// 'this' used in initializer list: yes, intentionally
#if LL_MSVC
# pragma warning (disable : 4355)
#endif

///////////////////////////////////////////////////////////////////////////////
// LLProcess class. It is implemented in its own llprocess.h and llprocess.cpp
// files in LL's viewer, but it is only used by LLLeap (we use the simpler and
// APR-free LLProcessLauncher everywhere else in the Cool VL Viewer), so there
// is no point in adding more files to the sources. HB
///////////////////////////////////////////////////////////////////////////////

// Utility functions.

static std::string get_desc(const LLSD& params)
{
	std::string desc;
	if (params.has("desc"))
	{
		desc = params["desc"].asString();
	}
	// Do not leave desc empty and use the filename (and maybe full path) for
	// the executable.
	if (desc.empty())
	{
		desc = params["executable"].asString();
	}
	return desc;
}

static std::string whichfile(S32 index)
{
	static const std::string stream_names[] =
	{
		"stdin", "stdout", "stderr"
	};
	if (index < 3)
	{
		return stream_names[index];
	}
	return llformat("file slot %d", index);
}

static std::string get_line_from_stream(std::istream& in)
{
	std::string line;
	std::getline(in, line);
	// Blur the distinction between "\r\n" and plain "\n". std::getline() will
	// have eaten the "\n", but we could still end up with a trailing "\r".
	std::string::size_type lastpos = line.find_last_not_of("\r");
	if (lastpos != std::string::npos)
	{
		// Found at least one character that's not a trailing '\r'. SKIP OVER
		// IT and erase the rest of the line.
		line.erase(lastpos + 1);
	}
	return line;
}

// LLProcess handles launching an external process with specified command line
// arguments. It also keeps track of whether the process is still running, and
// can kill it if required.
//
// In discussing LLProcess, we use the term "parent" to refer to this process
// (the process invoking LLProcess), versus "child" to refer to the process
// spawned by LLProcess.
//
// LLProcess relies on periodic post() calls on the "mainloop" LLEventPump: an
// LLProcess object's Status woould not update until the next "mainloop" tick.
// The viewer's main loop already posts to an LLEventPump by that name once per
// iteration.

class LLProcess final
{
protected:
	LOG_CLASS(LLProcess);

public:
	typedef std::shared_ptr<LLProcess> ptr_t;

	// Non-copyable
	LLProcess(const LLProcess&) noexcept = delete;
	LLProcess& operator=(const LLProcess&) noexcept = delete;

	~LLProcess();

	static ptr_t create(const LLSD& params);

	// State of child process
	enum state
	{
		UNSTARTED,	// Initial value, invisible to consumer
		RUNNING,	// Child process launched
		EXITED,		// Child process terminated voluntarily
		KILLED		// Child process terminated involuntarily
	};

	// Status info
	class Status
	{
	public:
		LL_INLINE Status()
		:	mState(UNSTARTED),
			mData(0)
		{
		}

		static Status interpretStatus(int status);

	public:
		state	mState;
		// For mState == EXITED: mData is exit() code
		// For mState == KILLED: mData is signal number (POSIX)
		// Otherwise: mData is undefined
		int		mData;
	};

	// Status query
	LL_INLINE Status getStatus() const			{ return mStatus; }

	LL_INLINE static Status getStatus(const ptr_t& p)
	{
		// Note: default-constructed Status has mState == UNSTARTED
		return p ? p->mStatus : Status();
	}

	// Is child process still running ?
	LL_INLINE bool isRunning() const
	{
		return mStatus.mState == RUNNING;
	}

	LL_INLINE static bool isRunning(const ptr_t& p)
	{
		return p && p->isRunning();
	}

	// Plain text status string query, for logging etc.
	LL_INLINE std::string getStatusString() const
	{
		return getStatusString(mStatus);
	}

	LL_INLINE static std::string getStatusString(const std::string& desc,
												 const ptr_t& p)
	{
		if (!p)
		{
			// Default-constructed Status has mState == UNSTARTED
			return getStatusString(desc, Status());
		}
		return desc + " " + p->getStatusString();
	}

	// Plain text status string query for previously-captured Status
	LL_INLINE std::string getStatusString(const Status& status) const
	{
		return getStatusString(mDesc, status);
	}

	// Plain text static status string query
	static std::string getStatusString(const std::string& desc,
									   const Status& status);

	// Attempts to kill the process. Returns true if the process is no longer
	// running when it returns./ Note that even if this returns false, the
	// process may exit some time after it is called.
	bool kill(const std::string& who = LLStringUtil::null);

	LL_INLINE static bool kill(const ptr_t& p,
							   const std::string& who = LLStringUtil::null)
	{
		return p && p->kill(who);
	}

#if LL_WINDOWS
	typedef int id;			// As returned by getProcessID()
	typedef HANDLE handle;	// As returned by getProcessHandle()
#else
	typedef pid_t id;
	typedef pid_t handle;
#endif
	// Gets an int-like id value. This is primarily intended for a human reader
	// to differentiate processes.
	LL_INLINE id getProcessID() const
	{
		return mProcess.pid;
	}

	// Gets a "handle" of a kind that you might pass to platform-specific API
	// functions to engage features not directly supported by LLProcess.
	LL_INLINE handle getProcessHandle() const
	{
#if LL_WINDOWS
		return mProcess.hproc;
#else
		return mProcess.pid;
#endif
	}

	// Tests if a process (handle obtained from getProcessHandle()) is still
	// running. Returns same nonzero handle value if still running, else zero,
	// so you can test it like a bool. But if you want to update a stored
	// variable as a side effect, you can write code like this:
	//	hchild = LLProcess::isRunning(hchild);
	//
	// Note: this method is intended as a unit-test hook, not as the first of
	// a whole set of operations supported on freestanding handle values. New
	// functionality should be added as non-static members operating on the
	// same data as getProcessHandle().
	//
	// In particular, if child termination is detected by this static
	// isRunning() rather than by nonstatic isRunning(), the LLProcess object
	// would not be aware of the child's changed status and may encounter OS
	// errors trying to obtain it. This static isRunning() is only intended for
	// after the launching LLProcess object has been destroyed.
	static handle isRunning(handle,
							const std::string& desc = LLStringUtil::null);

	// Provides symbolic access to child's file slots
	enum FILESLOT { STDIN = 0, STDOUT = 1, STDERR = 2, NSLOTS = 3 };

	// Base of ReadPipe, WritePipe
	class BasePipe
	{
	public:
		virtual ~BasePipe() = default;

		typedef size_t size_type;
		static const size_type npos;

		// Gets accumulated buffer length.
		//
		// For WritePipe, is there still pending data to send to child ?
		//
		// For ReadPipe, we often need to refrain from actually reading the
		// std::istream returned by get_istream() until we have accumulated
		// enough data to make it worthwhile. For instance, if we are expecting
		// a number from the child, but the child happens to flush "12" before
		// emitting "3\n", get_istream() >> myint could return 12 rather than
		// 123 !
		virtual size_type size() const = 0;
	};

	// As returned by getWritePipe() or getOptWritePipe()
	class WritePipe : public BasePipe
	{
	public:
		// Get ostream& on which to write to child's stdin. Usage:
		//  p->getWritePipe().get_ostream() << "Hello, child !" << std::endl;
		virtual std::ostream& get_ostream() = 0;
	};

	// As returned by getReadPipe() or getOptReadPipe()
	class ReadPipe : public BasePipe
	{
	public:
		// Gets istream& on which to read from child's stdout or stderr. Usage:
		// 	std::string stuff;
		// 	p->getReadPipe().get_istream() >> stuff;
		//
		// You should be sure in advance that the ReadPipe in question can fill
		// the request. See getPump()
		virtual std::istream& get_istream() = 0;

		// Like std::getline(get_istream(), line), but trims off trailing '\r'
		// to make calling code less platform-sensitive.
		virtual std::string getline() = 0;

		// Like get_istream().read(buffer, n), but returns std::string rather
		// than requiring caller to construct a buffer, etc.
		virtual std::string read(size_type len) = 0;

		// Peeks at accumulated buffer data without consuming it. Optional
		// parameters give you substr() functionality.
		//
		// Note: you can discard buffer data using get_istream().ignore(n).
		virtual std::string peek(size_type offset = 0,
								 size_type len = npos) const = 0;

		// Searches for a substring in accumulated buffer data without
		// retrieving it. Returns size_type position at which found, or npos
		// meaning not found. Optional offset allows you to search from
		// specified position.
		virtual size_type find(const std::string& seek,
							   size_type offset = 0) const = 0;

		// Detects the presence of a substring (or char) in accumulated buffer
		// data without retrieving it. Optional offset allows you to search
		// from specified position.
		template <typename SEEK>
		LL_INLINE bool contains(SEEK seek, size_type offset = 0) const
		{
			return find(seek, offset) != npos;
		}

		// Searches for a char in accumulated buffer data without retrieving
		// it. Returns size_type position at which found, or npos meaning not
		// found. Optional offset allows you to search from specified
		// position.
		virtual size_type find(char seek, size_type offset = 0) const = 0;

		// Gets LLEventPump& on which to listen for incoming data. The posted
		// LLSD::Map event will contain:
		//
		// - "data" part of pending data; see setLimit()
		// - "len" entire length of pending data, regardless of setLimit()
		// - "slot" this ReadPipe's FILESLOT, e.g. LLProcess::STDOUT
		// - "name" e.g. "stdout"
		// - "desc" e.g. "SLPlugin (pid) stdout"
		// - "eof" true means there no more data will arrive on this pipe,
		//   therefore no more events on this pump
		//
		// If the child sends "abc", and this ReadPipe posts "data"="abc", but
		// you do not consume it by reading the std::istream returned by
		// get_istream(), and the child next sends "def", ReadPipe will post
		// "data"="abcdef".
		virtual LLEventPump& getPump() = 0;

		// Sets maximum length of buffer data that will be posted in the LLSD
		// announcing arrival of new data from the child. If you call
		// setLimit(5), and the child sends "abcdef", the LLSD event will
		// contain "data"="abcde". However, you may still read the entire
		// "abcdef" from get_istream(): this limit affects only the size of
		// the data posted with the LLSD event. If you do not call this method,
		// no data will be posted: the default is 0 bytes.
		virtual void setLimit(size_type limit) = 0;

		// Query the current setLimit() limit.
		virtual size_type getLimit() const = 0;
	};

	// Exception thrown by getWritePipe(), getReadPipe() if you did not ask to
	// create a pipe at the corresponding FILESLOT.
	struct NoPipe : public std::runtime_error
	{
		LL_INLINE NoPipe(const std::string& what)
		:	std::runtime_error(what)
		{
		}

		LL_INLINE NoPipe(const char* what)
		:	std::runtime_error(what)
		{
		}
	};

	// Gets a reference to the (only) WritePipe for this LLProcess. 'slot', if
	// specified, must be STDIN. Throws NoPipe if you did not request a "pipe"
	// for child stdin.
	WritePipe& getWritePipe(FILESLOT slot = STDIN);

	// Gets a reference to one of the ReadPipes for this LLProcess. 'slot', if
	// specified, must be STDOUT or STDERR. Throws NoPipe if you did not
	// request a "pipe" for child stdout or stderr.
	ReadPipe& getReadPipe(FILESLOT slot);

	// Let's offer some introspection methods for LLLeap to retransmit to the
	// user code via its own methods. HB

	LL_INLINE const std::string& getDesc() const
	{
		return mDesc;
	}

	LL_INLINE const std::string& getExecutable() const
	{
		return mExecutable;
	}

	LL_INLINE const std::string& getInterpreter() const
	{
		return mInterpreter;
	}

	LL_INLINE const std::string& getCwd() const
	{
		return mCwd;
	}

	LL_INLINE const std::vector<std::string>& getArgs() const
	{
		return mAgs;
	}

private:
	// Use create() instead
	LLProcess(const LLSD& params);

	// Classic-C-style APR callback
	static void statusCallback(int reason, void* data, int status);
	// Object-oriented callback
	void handleStatus(int reason, int status);

	// Implementation for get[Read|Write]Pipe()
	template <class PIPETYPE>
	PIPETYPE& getPipe(FILESLOT slot);
	template <class PIPETYPE>
	PIPETYPE* getPipePtr(std::string& error, FILESLOT slot);

private:
	apr_pool_t*					mPool;
	std::string					mDesc;
	std::string					mExecutable;
#if !LL_WINDOWS
	std::string					mShebang;
#endif
	std::string					mInterpreter;
	std::string					mCwd;
	std::string					mPostend;

	std::vector<std::string>	mAgs;

	apr_proc_t					mProcess;
	// Explicitly want this ptr_vector to be able to store NULLs
	typedef boost::ptr_vector< boost::nullable<BasePipe> > pipe_vec_t;
	pipe_vec_t					mPipes;

	Status						mStatus;

	bool						mAttached;
};

// Support class internal to LLProcess: a ref-counted "mainloop" listener,
// which, as long as there are still outstanding LLProcess objects, keeps
// listening on "mainloop" so we can keep polling APR for process status.
class LLProcessListener final
{
protected:
	LOG_CLASS(LLProcessListener);

public:
	LL_INLINE LLProcessListener()
	:	mCount(0)
	{
	}

	void addPoll(const LLProcess&)
	{
		// Unconditionally increment mCount. If it was zero before
		// incrementing, listen on "mainloop".
		if (mCount++ == 0)
		{
			LL_DEBUGS("LLProcess") << "Listening on \"mainloop\"" << LL_ENDL;
			LLEventPump& pump = gEventPumps.obtain("mainloop");
			mConnection = pump.listen("LLProcessListener",
									  boost::bind(&LLProcessListener::tick,
												  this, _1));
		}
	}

	void dropPoll(const LLProcess&)
	{
		// Unconditionally decrement mCount. If it is zero after decrementing,
		// stop listening on "mainloop".
		if (--mCount == 0)
		{
			LL_DEBUGS("LLProcess") << "Disconnecting from \"mainloop\""
								   << LL_ENDL;
			mConnection.disconnect();
		}
	}

private:
	// Called once per frame by the "mainloop" LLEventPump
	bool tick(const LLSD&)
	{
		// Tell APR to sense whether each registered LLProcess is still running
		// and call handleStatus() appropriately. We should be able to get the
		// same info from an apr_proc_wait(APR_NOWAIT) call; but at least in
		// APR 1.4.2, testing suggests that even with APR_NOWAIT,
		// apr_proc_wait() blocks the caller. We cannot have that in the
		// viewer. Hence the callback rigmarole (once we update APR, it is
		// probably worth testing again). Also, although there is an
		// apr_proc_other_child_refresh() call, i.e. get that information for
		// one specific child, it accepts an 'apr_other_child_rec_t*' that is
		// mentioned NOWHERE else in the documentation or header files !  I
		// would use the specific call in LLProcess::getStatus() if I knew
		// how. As it is, each call to apr_proc_other_child_refresh_all() will
		// call callbacks for ALL still-running child processes. This is why we
		// centralize such calls, using "mainloop" to ensure it happens once
		// per frame, and refcounting running LLProcess objects to remain
		// registered only while needed.
		LL_DEBUGS("LLProcess") << "Calling apr_proc_other_child_refresh_all()"
							   << LL_ENDL;
		apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
		return false;
	}

private:
	// If this object is destroyed before mCount goes to zero, stop listening
	// on "mainloop" anyway.
	LLTempBoundListener	mConnection;

	S32					mCount;
};

static LLProcessListener sProcessListener;

// Use funky syntax to call max() to avoid blighted max() macros
const LLProcess::BasePipe::size_type LLProcess::BasePipe::npos =
	(std::numeric_limits<LLProcess::BasePipe::size_type>::max)();

class WritePipeImpl final : public LLProcess::WritePipe
{
protected:
	LOG_CLASS(WritePipeImpl);

public:
	WritePipeImpl(const std::string& desc, apr_file_t* pipe)
	:	mDesc(desc),
		mPipe(pipe),
		// We must initialize our std::ostream with our special streambuf !
		mStream(&mStreambuf)
	{
		LLEventPump& pump = gEventPumps.obtain("mainloop");
		mConnection = pump.listen(LLEventPump::inventName("WritePipe"),
								  boost::bind(&WritePipeImpl::tick, this, _1));
#if !LL_WINDOWS
		// We cannot count on every child process reading everything we try to
		// write to it. And if the child terminates with WritePipe data still
		// pending, unless we explicitly suppress it, POSIX will hit us with
		// SIGPIPE. That would terminate the viewer, boom. "Ignoring" it means
		// APR gets the correct errno, passes it back to us, we log it, etc.
		signal(SIGPIPE, SIG_IGN);
#endif
	}

	LL_INLINE std::ostream& get_ostream() override	{ return mStream; }
	LL_INLINE size_type size() const override		{ return mStreambuf.size(); }

	bool tick(const LLSD&)
	{
		typedef boost::asio::streambuf::const_buffers_type const_buff_seq_t;
		// If there is anything to send, try to send it.
		size_t total = mStreambuf.size();
		size_t consumed = 0;
		if (total)
		{
			const_buff_seq_t bufs = mStreambuf.data();
			// In general, our streambuf might contain a number of different
			// physical buffers; iterate over those.
			bool keepwriting = true;
			for (const_buff_seq_t::const_iterator it = bufs.begin(),
												  end = bufs.end();
				 it != end && keepwriting; ++it)
			{
				// Although apr_file_write() accepts const void*, we
				// manipulate const char* so we can increment the pointer.
				const char* remainptr =
					boost::asio::buffer_cast<const char*>(*it);
				size_t remainlen = boost::asio::buffer_size(*it);
				while (remainlen)
				{
					// Tackle the current buffer in discrete chunks. On
					// Windows, we have observed strange failures when trying
					// to write big lengths (~1 MB) in a single operation. Even
					// a 32K chunk seems too large. At some point along the way
					// apr_file_write() returns 11 (resource temporarily
					// unavailable, i.e. EAGAIN) and says it wrote 0 bytes,
					// even though it did write the chunk !  Our next write
					// attempt retries with the same chunk, resulting in the
					// chunk being duplicated at the child end. Using smaller
					// chunks is empirically more reliable.
					size_t towrite = llmin(remainlen, size_t(4096));
					apr_size_t written(towrite);
					apr_status_t err = apr_file_write(mPipe, remainptr,
													  &written);
					// EAGAIN is exactly what we want from a nonblocking pipe.
					// Rather than waiting for data, it should return
					// immediately.
					if (err != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(err))
					{
						llwarns << "apr_file_write(" << towrite << ") on "
								<< mDesc << " got " << err << llendl;
						ll_apr_warn_status(err);
					}

					// 'written' is modified to reflect the number of bytes
					// actually written. Make sure we consume those later (do
					// not consume them now, that would invalidate the buffer
					// iterator sequence !).
					consumed += written;
					// Do not forget to advance to next chunk of current buffer
					remainptr += written;
					remainlen -= written;

					LL_DEBUGS("LLProcess") << "Wrote " << written << " of "
										   << towrite << " bytes to " << mDesc
										   << " (original " << total << "),"
										   << " code " << err << ": ";
					char msgbuf[512];
					LL_CONT << apr_strerror(err, msgbuf, sizeof(msgbuf))
							<< LL_ENDL;

					// The parent end of this pipe is nonblocking. If we were
					// not able to write everything we wanted, do not keep
					// banging on it; that would not change until the child
					// reads some. Wait for next tick().
					if (written < towrite)
					{
						// Break outer loop over buffers too
						keepwriting = false;
						break;
					}
				}		// Next chunk of current buffer
			}			// Next buffer
			// In all, we managed to write 'consumed' bytes. Remove them from
			// the streambuf so we do not keep trying to send them. This could
			// be anywhere from 0 up to mStreambuf.size(); anything we have not
			// yet sent, we will try again later.
			mStreambuf.consume(consumed);
		}

		return false;
	}

private:
	std::string				mDesc;
	apr_file_t*				mPipe;
	LLTempBoundListener		mConnection;
	boost::asio::streambuf	mStreambuf;
	std::ostream			mStream;
};

class ReadPipeImpl final : public LLProcess::ReadPipe
{
protected:
	LOG_CLASS(ReadPipeImpl);

public:
	ReadPipeImpl(const std::string& desc, apr_file_t* pipe,
				 LLProcess::FILESLOT index)
	:	mDesc(desc),
		mPipe(pipe),
		mIndex(index),
		// We need to initialize our std::istream with our special streambuf !
		mStream(&mStreambuf),
		mPump("ReadPipe", true),	// Tweak name as needed to avoid collisions
		mLimit(0),
		mEOF(false)
	{
		LLEventPump& pump = gEventPumps.obtain("mainloop");
		mConnection = pump.listen(LLEventPump::inventName("ReadPipe"),
								  boost::bind(&ReadPipeImpl::tick, this, _1));
	}

	~ReadPipeImpl() override
	{
		if (mConnection.connected())
		{
			mConnection.disconnect();
		}
	}

	// Much of the implementation is simply connecting the abstract virtual
	// methods with implementation data concealed from the base class.
	LL_INLINE std::istream& get_istream() override	{ return mStream; }

	LL_INLINE std::string getline() override
	{
		return get_line_from_stream(mStream);
	}

	LL_INLINE LLEventPump& getPump() override		{ return mPump; }
	LL_INLINE void setLimit(size_type l) override	{ mLimit = l; }
	LL_INLINE size_type getLimit() const override	{ return mLimit; }
	LL_INLINE size_type size() const override		{ return mStreambuf.size(); }

	std::string read(size_type len) override
	{
		// Read specified number of bytes into a buffer.
		size_type readlen = llmin(size(), len);
		// Formally, &buffer[0] is invalid for a vector of size() 0. Exit
		// early in that situation.
		if (!readlen)
		{
			return "";
		}
		// Make a buffer big enough.
		std::vector<char> buffer(readlen);
		mStream.read(&buffer[0], readlen);
		// Since we have already clamped 'readlen', we can think of no reason
		// why mStream.read() should read fewer than 'readlen' bytes.
		// Nonetheless, use the actual retrieved length.
		return std::string(&buffer[0], mStream.gcount());
	}

	std::string peek(size_type offset = 0, size_type len = npos) const override
	{
		// Constrain caller's offset and len to overlap actual buffer content.
		size_t real_offset = llmin(mStreambuf.size(), size_t(offset));
		size_type want_end = len == npos ? npos : real_offset + len;
		size_t real_end	= llmin(mStreambuf.size(), size_t(want_end));
		boost::asio::streambuf::const_buffers_type cbufs = mStreambuf.data();
		return std::string(boost::asio::buffers_begin(cbufs) + real_offset,
						   boost::asio::buffers_begin(cbufs) + real_end);
	}

	size_type find(const std::string& seek, size_type off = 0) const override
	{
		// If we are passing a string of length 1, use find(char), which can
		// use an O(n) std::find() rather than the O(n^2) std::search().
		if (seek.length() == 1)
		{
			return find(seek[0], off);
		}

		// If off is beyond the whole buffer, we cannot even construct a valid
		// iterator range; cannot possibly find the string we seek.
		if (off > mStreambuf.size())
		{
			return npos;
		}

		typedef boost::asio::streambuf::const_buffers_type const_buff_type_t;
		const_buff_type_t cbufs = mStreambuf.data();
		boost::asio::buffers_iterator<const_buff_type_t> begin =
				boost::asio::buffers_begin(cbufs);
		boost::asio::buffers_iterator<const_buff_type_t> end =
			boost::asio::buffers_end(cbufs);
		boost::asio::buffers_iterator<const_buff_type_t> it =
			std::search(begin + off, end, seek.begin(), seek.end());
		return it == end ? npos : it - begin;
	}

	size_type find(char seek, size_type offset = 0) const override
	{
		// If offset is beyond the whole buffer, cannot even construct a valid
		// iterator range; cannot possibly find the char we seek.
		if (offset > mStreambuf.size())
		{
			return npos;
		}

		typedef boost::asio::streambuf::const_buffers_type const_buff_type_t;
		const_buff_type_t cbufs = mStreambuf.data();
		boost::asio::buffers_iterator<const_buff_type_t> begin =
			boost::asio::buffers_begin(cbufs);
		boost::asio::buffers_iterator<const_buff_type_t> end =
			boost::asio::buffers_end(cbufs);
		boost::asio::buffers_iterator<const_buff_type_t> it =
			std::find(begin + offset, end, seek);
		return it == end ? npos : it - begin;
	}

	bool tick(const LLSD&)
	{
		// Once we have hit EOF, skip all the rest of this.
		if (mEOF)
		{
			return false;
		}

		typedef boost::asio::streambuf::mutable_buffers_type mut_buff_seq_t;
		// Try, every time, to read into our streambuf. In fact, we have no
		// idea how much data the child might be trying to send: keep trying
		// until we are convinced we have temporarily exhausted the pipe.
		enum PipeState { RETRY, EXHAUSTED, CLOSED };
		PipeState state = RETRY;
		size_t committed = 0;
		do
		{
			// Attempt to read an arbitrary size
			mut_buff_seq_t bufs = mStreambuf.prepare(4096);
			// In general, the mut_buff_seq_t returned by prepare() might
			// contain a number of different physical buffers; iterate over
			// those.
			size_t tocommit = 0;
			for (mut_buff_seq_t::const_iterator it = bufs.begin(),
												end = bufs.end();
				 it != end; ++it)
			{
				size_t toread = boost::asio::buffer_size(*it);
				apr_size_t gotten = toread;
				apr_status_t err =
					apr_file_read(mPipe, boost::asio::buffer_cast<void*>(*it),
								  &gotten);
				// EAGAIN is exactly what we want from a nonblocking pipe.
				// Rather than waiting for data, it should return immediately.
				if (err != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(err))
				{
					// Handle EOF specially: it is part of normal-case
					// processing.
					if (err == APR_EOF)
					{
						LL_DEBUGS("LLProcess") << "EOF on " << mDesc
											   << LL_ENDL;
					}
					else
					{
						llwarns << "apr_file_read(" << toread << ") on "
								<< mDesc << " got " << err << llendl;
						ll_apr_warn_status(err);
					}
					// Either way, though, we would not need any more tick()
					// calls.
					mConnection.disconnect();
					// Ignore any subsequent calls we might get anyway.
					mEOF = true;
					state = CLOSED; // Also break outer retry loop
					break;
				}

				// 'gotten' was modified to reflect the number of bytes
				// actually received. Make sure we commit those later (do not
				// commit them now, that would invalidate the buffer iterator
				// sequence !).
				tocommit += gotten;
				LL_DEBUGS("LLProcess") << "Filled " << gotten << " of "
									   << toread << " bytes from " << mDesc
									   << LL_ENDL;

				// The parent end of this pipe is nonblocking. If we were not
				// even able to fill this buffer, do not loop to try to fill
				// the next; that would not change until the child writes more.
				// Wait for next tick().
				if (gotten < toread)
				{
					state = EXHAUSTED;	// Break outer retry loop too
					break;
				}
			}

			// Do not forget to "commit" the data !
			mStreambuf.commit(tocommit);
			committed += tocommit;

			// 'state' is changed from RETRY when we cannot fill any one buffer
			// of the mut_buff_seq_t established by the current prepare() call,
			// whether due to error or not enough bytes.
			// That is, if state is still RETRY, we have filled every physical
			// buffer in the mut_buff_seq_t. In that case, for all we know, the
			// child might have still more data pending: go for it !
		}
		while (state == RETRY);

		// Once we recognize that the pipe is closed, make one more call to
		// listener. The listener might be waiting for a particular substring
		// to arrive, or a particular length of data or something. The event
		// with "eof" == true announces that nothing further will arrive, so
		// use it or lose it.
		if (committed || state == CLOSED)
		{
			// If we actually received new data, publish it on our LLEventPump
			// as advertised. Constrain it by mLimit. But show listener the
			// actual accumulated buffer size, regardless of mLimit.
			size_type datasize(llmin(mLimit, size_type(mStreambuf.size())));
			mPump.post(LLSDMap("data", peek(0, datasize))
							  ("len", LLSD::Integer(mStreambuf.size()))
							  ("slot", LLSD::Integer(mIndex))
							  ("name", whichfile(mIndex))
							  ("desc", mDesc)
							  ("eof", state == CLOSED));
		}

		return false;
	}

private:
	std::string				mDesc;
	apr_file_t*				mPipe;
	LLTempBoundListener		mConnection;
	LLEventStream			mPump;
	boost::asio::streambuf	mStreambuf;
	std::istream			mStream;
	LLProcess::FILESLOT		mIndex;
	size_type				mLimit;
	bool					mEOF;
};

LLProcess::ptr_t LLProcess::create(const LLSD& params)
{
	try
	{
		return ptr_t(new LLProcess(params));
	}
	catch (const LLLeap::Error& e)
	{
		llwarns << e.what() << llendl;

		// If caller is requesting an event on process termination, send one
		// indicating bad launch. This may prevent someone waiting forever for
		// a termination post that cannot arrive because the child never
		// started.
		if (params.has("postend"))
		{
			gEventPumps.obtain(params["postend"].asString())
				.post(LLSDMap("desc", get_desc(params))			// No "id"
							 ("state", LLProcess::UNSTARTED)
							 // No "data"
							 ("string", e.what()));
		}
		return ptr_t();
	}
}

// Calls an APR function returning apr_status_t. On failure, logs a warning and
// throws LLLeap::Error mentioning the function call that produced that result.
#define chkapr(func)							\
	if (ll_apr_warn_status(func)) { throw LLLeap::Error(#func " failed"); }

LLProcess::LLProcess(const LLSD& params)
:	mPool(NULL),
	mPipes(NSLOTS)
{
	if (!params.isMap() || !params.has("executable"))
	{
		throw LLLeap::Error("process not launched: failed parameter validation");
	}

	// Create an argv vector for the child process; APR wants a vector of
	// pointers to constant C strings (see the note below about it).
	std::vector<const char*> argv;

	// In preparation for calling apr_proc_create(), we collect a number of
	// const char* pointers obtained from std::string::c_str().
	// Note: we store the parameters in member variable strings so that we can
	// guarantee (as long as a C++11 compiler and standard library are used)
	// that c_str() returns a pointer that will stay valid and constant during
	// the whole LLProcess instance life. HB
	mExecutable = params["executable"].asString();
#if LL_WINDOWS
	// Replace Windows path separators with UNIX ones that APR can understand.
	LLStringUtil::replaceChar(mExecutable, '\\', '/');

	std::string lcname = mExecutable;
	size_t len = lcname.size();
	LLStringUtil::toLower(lcname);
	if (lcname.rfind(".exe") != len - 4 && lcname.rfind(".com") != len - 4)
	{
		// If we are not passed an executable, and since Windows does not
		// honour shebang lines in scripts, try and find an adequate
		// interpreter for the script file.
		llinfos << "File " << mExecutable
				<< " is apparently not a Windows executable..." << llendl;
		if (lcname.rfind(".py") == len - 3)
		{
			llinfos << "Python script assumed, based on extension: trying 'pythonw.exe' to interpret it."
					<< llendl;
			mInterpreter = "pythonw.exe";
			argv.emplace_back(mInterpreter.c_str());
		}
		else if (lcname.rfind(".bat") == len - 4 ||
				 lcname.rfind(".cmd") == len - 4)
		{
			llinfos << "Batch script assumed, based on extension: trying 'cmd.exe' to interpret it."
					<< llendl;
			mInterpreter = "cmd.exe";
		}
		else if (lcname.rfind(".lua") == len - 4)
		{
			llinfos << "Lua script assumed, based on extension: trying 'lua.exe' to interpret it."
					<< llendl;
			mInterpreter = "lua.exe";
		}
		else
		{
			llwarns << "Not a known/supported script type: expect a failure..."
					<< llendl;
		}
	}
#else	// POSIX
	// If we do have a full path, but the file is not executable, it could
	// still be a valid script (but with the exec bit not set). Since APR would
	// refuse to launch a non-executable script, try and find by ourselves a
	// shebang line and use it to launch the proper executable for that
	// script... HB
	llstat st;
	if (LLFile::stat(mExecutable, &st) == 0 && !(st.st_mode & S_IEXEC))
	{
		llwarns << "File " << mExecutable << " is not executable." << llendl;
		llifstream script(mExecutable.c_str());
		std::getline(script, mShebang);
		if (mShebang.size() > 3 && mShebang.compare(0, 2, "#!") == 0)
		{
			llinfos << "Found a shebang; assumed to be a script." << llendl;
			mInterpreter = mShebang.substr(2);	// Remove the leading "#!"
			LLStringUtil::trim(mInterpreter);	// Trim spaces
			size_t i = mInterpreter.find(' ');	// E.g.: /usr/bin/env python
			if (i != std::string::npos)
			{
				// This is the actual executable (e.g. /usr/bin/env), so
				// place it in first position in the list of arguments. See
				// above remark about c_str().
				mShebang = mInterpreter.substr(0, i);
				argv.emplace_back(mShebang.c_str());
				// Get the interpreter name...
				mInterpreter = mInterpreter.substr(i + 1);
				LLStringUtil::trimHead(mInterpreter);	// Trim spaces
			}
		}
	}
#endif
	// Add any interpreter into the arguments list in first (or second, for
	// scripts using a shebang line with /usr/bin/env or such) position. See
	// above remark about c_str().
	if (!mInterpreter.empty())
	{
		llinfos << "Attempting to use the following intrepreter program: "
				<< mInterpreter << llendl;
		argv.emplace_back(mInterpreter.c_str());
	}
	// We may now add the "executable" (or script) path. See above remark
	// about c_str().
	argv.emplace_back(mExecutable.c_str());

	// Hmm, when you construct a ptr_vector with a size, it merely reserves
	// space, it does not actually make it that big. Explicitly make it bigger.
	// Because of ptr_vector's odd semantics, have to push_back(0) the right
	// number of times !  resize() wants to default-construct new BasePipe
	// instances, which fails because it's pure virtual. But because of the
	// constructor call, these push_back() calls should require no new
	// allocation.
	for (size_t i = 0, count = mPipes.capacity(); i < count; ++i)
	{
		mPipes.push_back(0);
	}

	mAttached = params.has("attached") && params["attached"].asBoolean();

	if (params.has("postend"))
	{
		mPostend = params["postend"].asString();
	}

	apr_pool_create(&mPool, gAPRPoolp);
	if (!mPool)
	{
		throw LLLeap::Error("APR pool creation failed");
	}

	apr_procattr_t* procattr = NULL;
	chkapr(apr_procattr_create(&procattr, mPool));

	// IQA-490, CHOP-900: on Windows, ask APR to jump through hoops to
	// constrain the set of handles passed to the child process. Before we
	// changed to APR, the Windows implementation of LLProcessLauncher called
	// CreateProcess(bInheritHandles=FALSE), meaning to pass NO open handles
	// to the child process. Now that we support pipes, though, we must allow
	// apr_proc_create() to pass bInheritHandles=TRUE. But without taking
	// special pains, that causes trouble in a number of ways, due to the fact
	// that the viewer is constantly opening and closing files: most of which
	// CreateProcess() passes to every child process!
#if defined(APR_HAS_PROCATTR_CONSTRAIN_HANDLE_SET)
	chkapr(apr_procattr_constrain_handle_set(procattr, 1));
#else
	// Our special preprocessor symbol is not even defined: wrong APR !
	llwarns << "This version of APR lacks Linden "
			<< "apr_procattr_constrain_handle_set() extension" << llendl;
#endif

	// For which of stdin, stdout, stderr should we create a pipe to the
	// child ?  In the viewer, there are only a couple viable
	// apr_procattr_io_set() alternatives: inherit the viewer's own stdxxx
	// handle (APR_NO_PIPE, e.g. for stdout, stderr), or create a pipe that is
	// blocking on the child end but nonblocking at the viewer end
	// (APR_CHILD_BLOCK).
	// Other major options could include explicitly creating a single APR pipe
	// and passing it as both stdout and stderr (apr_procattr_child_out_set(),
	// apr_procattr_child_err_set()), or accepting a filename, opening it and
	// passing that apr_file_t (simple <, >, 2> redirect emulation).
	std::vector<apr_int32_t> select;
	if (params.has("files"))
	{
		const LLSD& files = params["files"];
		if (files.isArray())
		{
			for (LLSD::array_const_iterator it = files.beginArray(),
											end = files.endArray();
				 it != end; ++it)
			{
				if (it->asString() == "pipe")
				{
					select.emplace_back(APR_CHILD_BLOCK);
				}
				// Ignore all unsupported file types and consider no file
				// redirection was asked for them. HB
				else
				{
					select.emplace_back(APR_NO_PIPE);
				}
			}
		}
	}
	// By default, pass APR_NO_PIPE for unspecified slots.
	while (select.size() < NSLOTS)
	{
		select.push_back(APR_NO_PIPE);
	}
	chkapr(apr_procattr_io_set(procattr, select[STDIN], select[STDOUT],
							   select[STDERR]));

	// Thumbs down on implicitly invoking the shell to invoke the child. From
	// our point of view, the other major alternative to APR_PROGRAM_PATH would
	// be APR_PROGRAM_ENV: still copy environment, but require full executable
	// pathname. There is no real downside to searching the PATH, though: if
	// our caller wants (e.g.) a specific Python interpreter, they can still
	// pass the full pathname.
	chkapr(apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH));
	// YES, do extra work if necessary to report child exec() failures back to
	// parent process.
	chkapr(apr_procattr_error_check_set(procattr, 1));

	if (params.has("cwd"))
	{
		mCwd = params["cwd"].asString();
		chkapr(apr_procattr_dir_set(procattr, mCwd.c_str()));
	}

	if (params.has("args"))
	{
		const LLSD& args = params["args"];
		if (args.isArray())
		{
			for (LLSD::array_const_iterator it = args.beginArray(),
											end = args.endArray();
				 it != end; ++it)
			{
				mAgs.emplace_back(it->asString());
				// See above remark about c_str().
				argv.emplace_back(mAgs.back().c_str());
			}
		}
	}

	// Terminate with a null pointer
	argv.push_back(NULL);

	// Launch !  The NULL would be the environment block, if we were passing
	// one. Hand-expand chkapr() macro so we can fill in the actual command
	// string instead of the variable names.
	if (ll_apr_warn_status(apr_proc_create(&mProcess, argv[0], &argv[0], NULL,
										   procattr, mPool)))
	{
		throw LLLeap::Error(get_desc(params) + " failed");
	}

	// Arrange to call statusCallback()
	apr_proc_other_child_register(&mProcess, &LLProcess::statusCallback, this,
								  mProcess.in, mPool);
	// And make sure we poll it once per "mainloop" tick
	sProcessListener.addPoll(*this);
	mStatus.mState = RUNNING;

	mDesc = llformat("%s (%d)", get_desc(params).c_str(), mProcess.pid);
	llinfos << mDesc << ": launched " << params << llendl;

	// Instantiate the proper pipe I/O machinery: we want to be able to point
	// to apr_proc_t::in, out, err by index
	typedef apr_file_t* apr_proc_t::*apr_proc_file_ptr;
	static apr_proc_file_ptr members[] =
	{
		&apr_proc_t::in,
		&apr_proc_t::out,
		&apr_proc_t::err
	};
	for (size_t i = 0; i < NSLOTS; ++i)
	{
		if (select[i] != APR_CHILD_BLOCK)
		{
			continue;
		}
		std::string desc = mDesc + " " + whichfile(FILESLOT(i));
		apr_file_t* pipe(mProcess.*(members[i]));
		if (i == STDIN)
		{
			mPipes.replace(i, new WritePipeImpl(desc, pipe));
		}
		else
		{
			mPipes.replace(i, new ReadPipeImpl(desc, pipe, FILESLOT(i)));
		}
	}
}

LLProcess::~LLProcess()
{
	// Unregistering is pointless (and fatal) when this is called after the
	// global APR pool has been destroyed on viewer exit, and kill(), which
	// also relies on APR, is impossible anyway.
	if (!mPool)
	{
		return;
	}

	// Only in state RUNNING are we registered for callback. In UNSTARTED we
	// have not yet registered. And since receiving the callback is the only
	// way we detect child termination, we only change from state RUNNING at
	// the same time we unregister.
	if (mStatus.mState == RUNNING)
	{
		// We are still registered for a callback: unregister. Do it before
		// we even issue the kill(): even if kill() somehow prompted an
		// instantaneous callback (unlikely), this object is going away !  Any
		// information updated in this object by such a callback is no longer
		// available to any consumer anyway.
		apr_proc_other_child_unregister(this);
		// One less LLProcess to poll for
		sProcessListener.dropPoll(*this);
	}

	if (mAttached)
	{
		kill("destructor");
	}

	apr_pool_destroy(mPool);
	mPool = NULL;
}

bool LLProcess::kill(const std::string& who)
{
	if (isRunning())
	{
		llinfos << who << " killing " << mDesc << llendl;

#if LL_WINDOWS
		int sig = -1;
#else  // POSIX
		int sig = SIGTERM;
#endif
		ll_apr_warn_status(apr_proc_kill(&mProcess, sig));
	}

	return !isRunning();
}

//static
std::string LLProcess::getStatusString(const std::string& desc,
									   const Status& status)
{
	if (status.mState == UNSTARTED)
	{
		return desc + " was never launched";
	}

	if (status.mState == RUNNING)
	{
		return desc + " running";
	}

	if (status.mState == EXITED)
	{
		return llformat("%s exited with code %d", desc.c_str(), status.mData);
	}

	if (status.mState == KILLED)
	{
#if LL_WINDOWS
		return llformat("%s killed with exception %x", desc.c_str(),
						status.mData);
#else
		return llformat("%s killed by signal %d (%s)", desc.c_str(),
						status.mData,
						apr_signal_description_get(status.mData));
#endif
	}

	return llformat("%s in unknown state %d (%d)", desc.c_str(), status.mState,
					status.mData);
}

// Classic-C-style APR callback
void LLProcess::statusCallback(int reason, void* data, int status)
{
	// Our only role is to bounce this static method call back into object
	// space.
	((LLProcess*)data)->handleStatus(reason, status);
}

#define tabent(symbol) { symbol, #symbol }
static struct ReasonCode
{
	int code;
	const char* name;
} reasons[] =
{
	tabent(APR_OC_REASON_DEATH),
	tabent(APR_OC_REASON_UNWRITABLE),
	tabent(APR_OC_REASON_RESTART),
	tabent(APR_OC_REASON_UNREGISTER),
	tabent(APR_OC_REASON_LOST),
	tabent(APR_OC_REASON_RUNNING)
};
#undef tabent

// Object-oriented callback
void LLProcess::handleStatus(int reason, int status)
{
	LL_DEBUGS("LLProcess") << mDesc << ": handleStatus(";
	std::string reason_str;
	for (const ReasonCode& rcp : reasons)
	{
		if (reason == rcp.code)
		{
			reason_str = rcp.name;
			break;
		}
	}
	if (reason_str.empty())
	{
		reason_str = llformat("unknown reason %d", reason);
	}
	LL_CONT << reason_str << ", " << status << ")" << LL_ENDL;

	if (reason != APR_OC_REASON_DEATH && reason != APR_OC_REASON_LOST)
	{
		// We are only interested in the call when the child terminates.
		return;
	}

	// Somewhat oddly, APR requires that you explicitly unregister even when
	// it already knows the child has terminated. We must pass the same 'data'
	// pointer as for the register() call, which was our 'this'.
	apr_proc_other_child_unregister(this);
	// Do not keep polling for a terminated process
	sProcessListener.dropPoll(*this);
	// We overload mStatus.mState to indicate whether the child is registered
	// for APR callback: only RUNNING means registered. Track that we have
	// unregistered. We know the child has terminated; might be EXITED or
	// KILLED; refine below.
	mStatus.mState = EXITED;

	// Make last-gasp calls for each of the ReadPipes we have on hand. Since
	// they are listening on "mainloop", we can be sure they will eventually
	// collect all pending data from the child. But we want to be able to
	// guarantee to our consumer that by the time we post on the "postend"
	// LLEventPump, our ReadPipes are already buffering all the data there
	// will ever be from the child. That lets the "postend" listener decide
	// what to do with that final data.
	std::string error;
	for (size_t i = 0; i < mPipes.size(); ++i)
	{
		ReadPipeImpl* ppipe = getPipePtr<ReadPipeImpl>(error, FILESLOT(i));
		if (ppipe)
		{
			static LLSD trivial;
			ppipe->tick(trivial);
		}
	}

	// wi->rv = apr_proc_wait(wi->child, &wi->rc, &wi->why, APR_NOWAIT);
	// It is just wrong to call apr_proc_wait() here. The only way APR knows to
	// call us with APR_OC_REASON_DEATH is that it has already reaped this
	// child process, so calling wait() will only produce "huh ?" from the OS.
	// We must rely on the status param passed in, which unfortunately comes
	// straight from the OS wait() call, which means we have to decode it by
	// hand.
	mStatus = Status::interpretStatus(status);
	llinfos << getStatusString() << llendl;

	if (mPostend.empty())
	{
		return;	// We are done
	}
	// If the caller requested notification on child termination, send it.
	gEventPumps.obtain(mPostend).post(LLSDMap("id",     getProcessID())
											 ("desc",   mDesc)
											 ("state",  mStatus.mState)
											 ("data",   mStatus.mData)
											 ("string", getStatusString()));
}

template<class PIPETYPE>
PIPETYPE* LLProcess::getPipePtr(std::string& error, FILESLOT slot)
{
	if (slot >= NSLOTS)
	{
		error = llformat("%s has no slot %d", mDesc.c_str(), slot);
		return NULL;
	}
	if (mPipes.is_null(slot))
	{
		error = mDesc + " " + whichfile(slot) + " is not a monitored pipe";
		return NULL;
	}
	// Make sure we dynamic_cast in pointer domain so we can test, rather than
	// accepting runtime's exception.
	PIPETYPE* ppipe = dynamic_cast<PIPETYPE*>(&mPipes[slot]);
	if (!ppipe)
	{
		error = mDesc + " " + whichfile(slot) + " is not a " +
				typeid(PIPETYPE).name();
		return NULL;
	}

	error.clear();
	return ppipe;
}

template <class PIPETYPE>
PIPETYPE& LLProcess::getPipe(FILESLOT slot)
{
	std::string error;
	PIPETYPE* wp = getPipePtr<PIPETYPE>(error, slot);
	if (!wp)
	{
		throw NoPipe(error);
	}
	return *wp;
}

LLProcess::WritePipe& LLProcess::getWritePipe(FILESLOT slot)
{
	return getPipe<WritePipe>(slot);
}

LLProcess::ReadPipe& LLProcess::getReadPipe(FILESLOT slot)
{
	return getPipe<ReadPipe>(slot);
}

#if LL_WINDOWS

LLProcess::handle LLProcess::isRunning(handle h, const std::string& desc)
{
	// This direct Windows implementation is because we have no access to the
	// apr_proc_t struct: we expect it has been destroyed.
	if (!h)
	{
		return 0;
	}

	DWORD waitresult = WaitForSingleObject(h, 0);
	if (waitresult == WAIT_OBJECT_0)
	{
		// The process has completed.
		if (!desc.empty())
		{
			DWORD status = 0;
			if (!GetExitCodeProcess(h, &status))
			{
				llwarns << desc
						<< " terminated, but GetExitCodeProcess() failed with error code: "
						<< GetLastError() << llendl;
			}
			else
			{
				llinfos << getStatusString(desc,
										   Status::interpretStatus(status))
						<< llendl;
			}
		}
		CloseHandle(h);
		return 0;
	}

	return h;
}

//static
LLProcess::Status LLProcess::Status::interpretStatus(int status)
{
	LLProcess::Status result;

	// This bit of code is cribbed from apr/threadproc/win32/proc.c, a function
	// (unfortunately static) called why_from_exit_code(). See WinNT.h
	// STATUS_ACCESS_VIOLATION and family for how this class of failures was
	// determined
	if ((status & 0xFFFF0000) == 0xC0000000)
	{
		result.mState = LLProcess::KILLED;
	}
	else
	{
		result.mState = LLProcess::EXITED;
	}
	result.mData = status;

	return result;
}

#else // Mac and linux

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

// Attempts to reap a process ID. Returns true if the process has exited and
// been reaped, false otherwise.
static bool reap_pid(pid_t pid, LLProcess::Status* pstatus = NULL)
{
	LLProcess::Status dummy;
	if (!pstatus)
	{
		// If caller does not want to see Status, give us a target anyway so we
		// do not have to have a bunch of conditionals.
		pstatus = &dummy;
	}

	int status = 0;
	pid_t wait_result = ::waitpid(pid, &status, WNOHANG);
	if (wait_result == pid)
	{
		*pstatus = LLProcess::Status::interpretStatus(status);
		return true;
	}
	if (wait_result == 0)
	{
		pstatus->mState = LLProcess::RUNNING;
		pstatus->mData	= 0;
		return false;
	}

	// Clear caller's Status block; caller must interpret UNSTARTED to mean
	// "if this PID was ever valid, it no longer is."
	*pstatus = LLProcess::Status();

	// We have dealt with the success cases: we were able to reap the child
	// (wait_result == pid) or it is still running (wait_result == 0). It may
	// be that the child terminated but did not hang around long enough for us
	// to reap. In that case we still have no Status to report, but we can at
	// least state that it's not running.
	if (wait_result == -1 && errno == ECHILD)
	{
		// No such process: this may mean we are ignoring SIGCHILD.
		return true;
	}

	// Uh, should never happen ?!
	llwarns << "waitpid(" << pid << ") returned " << wait_result
			<< "; not meaningful ?" << llendl;
	// If caller is looping until this pid terminates, and if we cannot find
	// out, better to break the loop than to claim it is still running.
	return true;
}

LLProcess::id LLProcess::isRunning(id pid, const std::string& desc)
{
	// This direct POSIX implementation is because we have no access to the
	// apr_proc_t struct: we expect it has been destroyed.
	if (!pid)
	{
		return 0;
	}

	// Check whether the process has exited, and reap it if it has.
	Status status;
	if (reap_pid(pid, &status))
	{
		// The process has exited.
		if (!desc.empty())
		{
			std::string str;
			// We do not just pass UNSTARTED to getStatusString() because, in
			// the context of reap_pid(), that state has special meaning.
			if (status.mState != UNSTARTED)
			{
				str = getStatusString(desc, status);
			}
			else
			{
				str = desc + " apparently terminated: no status available";
			}
			llinfos << str << llendl;
		}
		return 0;
	}

	return pid;
}

//static
LLProcess::Status LLProcess::Status::interpretStatus(int status)
{
	LLProcess::Status result;

	if (WIFEXITED(status))
	{
		result.mState = LLProcess::EXITED;
		result.mData  = WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status))
	{
		result.mState = LLProcess::KILLED;
		result.mData  = WTERMSIG(status);
	}
	else	// Should not happen
	{
		result.mState = LLProcess::EXITED;
		result.mData  = status;	 // Someone else will have to decode
	}

	return result;
}

#endif  // POSIX

///////////////////////////////////////////////////////////////////////////////
// LLLeapListener class. It is implemented in its own llleaplistener.h and
// llleaplistener.cpp files in LL's viewer, but it is only used internally to
// LLLeap, so there is no point in adding more files to the sources. HB
///////////////////////////////////////////////////////////////////////////////

class LLLeapListener final : public LLEventAPI
{
public:
	// Decouple LLLeap by dependency injection. Certain LLLeapListener
	// operations must be able to cause LLLeap to listen on a specified
	// LLEventPump with the LLLeap listener that wraps incoming events in an
	// outer (pump=, data=) map and forwards them to the plugin. Very well,
	// define the signature for a function that will perform that, and make
	// our constructor accept such a function.
	typedef boost::function<LLBoundListener(LLEventPump&,
											const std::string& listener)>
			connect_func_t;

	LLLeapListener(const connect_func_t& connect);
	~LLLeapListener() override;

	// The LLSD map obtained via getFeatures() is intended to be machine-
	// readable (read: easily-parsed, if parsing be necessary) and to highlight
	// the differences between this version of the LEAP protocol and the base
	//line version. A client may thus determine whether or not the running
	// viewer supports some recent feature of interest.
	//
	// This method is defined at the top of this implementation file so it is
	// easy to find, easy to spot, easy to update as we enhance the LEAP
	// protocol.
	static LLSD getFeatures()
	{
		static LLSD features;
		if (features.isUndefined())
		{
			// This initial implementation IS the baseline LEAP protocol; thus
			// the set of differences is empty; thus features is initially
			// empty. Expand with: features["featurename"] = "value";
			features = LLSD::emptyMap();
		}
		return features;
	}

private:
	void newPump(const LLSD&);
	void listen(const LLSD&);
	void stopListening(const LLSD&);
	void ping(const LLSD&) const;
	void getAPIs(const LLSD&) const;
	void getAPI(const LLSD&) const;
	void getFeatures(const LLSD&) const;
	void getFeature(const LLSD&) const;

	void saveListener(const std::string& pump_name,
					  const std::string& listener_name,
					  const LLBoundListener& listener);

private:
	connect_func_t	mConnect;

	// In theory, listen() could simply call the relevant LLEventPump's
	// listen() method, stopListening() likewise. Lifespan issues make us
	// capture the LLBoundListener objects: when this object goes away, all
	// those listeners should be disconnected. But what if the client listens,
	// stops, listens again on the same LLEventPump with the same listener
	// name ?  Merely collecting LLBoundListeners would not adequately track
	// that. So capture the latest LLBoundListener for this LLEventPump name
	// and listener name.
	typedef std::map<std::pair<std::string, std::string>,
					 LLBoundListener> listen_map_t;
	listen_map_t	mListeners;
};

LLLeapListener::LLLeapListener(const connect_func_t& connect)
	// Each LEAP plugin has an instance of this listener. Make the command
	// pump name difficult for other such plugins to guess.
:	LLEventAPI(LLUUID::generateNewID().asString(),
			   "Operations relating to the LLSD Event API Plugin (LEAP) protocol"),
	mConnect(connect)
{
	LLSD need_name(LLSDMap("name", LLSD()));
	add("newpump",
		"Instantiate a new LLEventPump named like [\"name\"] and listen to it.\n"
		"[\"type\"] == \"LLEventStream\", \"LLEventMailDrop\" et al.\n"
		"Events sent through new LLEventPump will be decorated with [\"pump\"]=name.\n"
		"Returns actual name in [\"name\"] (may be different if collision).",
		&LLLeapListener::newPump, need_name);

	LLSD need_source_listener(LLSDMap("source", LLSD())("listener", LLSD()));
	add("listen",
		"Listen to an existing LLEventPump named [\"source\"], with listener name\n"
		"[\"listener\"].\n"
		"By default, send events on [\"source\"] to the plugin, decorated\n"
		"with [\"pump\"]=[\"source\"].\n"
		"If [\"dest\"] specified, send undecorated events on [\"source\"] to the\n"
		"LLEventPump named [\"dest\"].\n"
		"Returns [\"status\"] boolean indicating whether the connection was made.",
		&LLLeapListener::listen, need_source_listener);
	add("stoplistening",
		"Disconnect a connection previously established by \"listen\".\n"
		"Pass same [\"source\"] and [\"listener\"] arguments.\n"
		"Returns [\"status\"] boolean indicating whether such a listener existed.",
		&LLLeapListener::stopListening,
		need_source_listener);

	add("ping",
		"No arguments, just a round-trip sanity check.",
		&LLLeapListener::ping);

	add("getAPIs",
		"Enumerate all LLEventAPI instances by name and description.",
		&LLLeapListener::getAPIs);
	add("getAPI",
		"Get name, description, dispatch key and operations for LLEventAPI [\"api\"].",
		&LLLeapListener::getAPI,
		LLSD().with("api", LLSD()));

	add("getFeatures",
		"Return an LLSD map of feature strings (deltas from baseline LEAP protocol)",
		static_cast<void (LLLeapListener::*)(const LLSD&) const>(&LLLeapListener::getFeatures));
	add("getFeature",
		"Return the feature value with key [\"feature\"]",
		&LLLeapListener::getFeature, LLSD().with("feature", LLSD()));
}

LLLeapListener::~LLLeapListener()
{
	// We wouldd have stored a map of LLTempBoundListener instances, save that
	// the operation of inserting into a std::map necessarily copies the
	// value_type, and Bad Things would happen if you copied a
	// LLTempBoundListener (destruction of the original would disconnect the
	// listener, invalidating every stored connection).
	for (listen_map_t::value_type& pair : mListeners)
	{
		pair.second.disconnect();
	}
}

void LLLeapListener::newPump(const LLSD& request)
{
	Response reply(LLSD(), request);
	std::string name = request["name"];
	std::string type = request["type"];

	try
	{
		// Tweak the name for uniqueness
		LLEventPump& new_pump(gEventPumps.make(name, true, type));
		name = new_pump.getName();
		reply["name"] = name;
		// Now listen on this new pump with our plugin listener
		std::string myname = "llleap";
		saveListener(name, myname, mConnect(new_pump, myname));
	}
	catch (const LLEventPumps::BadType& error)
	{
		reply.error(error.what());
	}
}

void LLLeapListener::listen(const LLSD& request)
{
	Response reply(LLSD(), request);
	reply["status"] = false;

	std::string source_name = request["source"];
	LLEventPump& source = gEventPumps.obtain(source_name);

	std::string listener_name = request["listener"];
	if (mListeners.count(listen_map_t::key_type(source_name,
											   listener_name)))
	{
		// We are already listening at that source...
		return;
	}

	std::string dest_name = request["dest"];

	try
	{
		if (request["dest"].isDefined())
		{
			// If we are asked to connect the "source" pump to a specific
			// "dest" pump, find dest pump and connect it.
			LLEventPump& dest = gEventPumps.obtain(dest_name);
			saveListener(source_name, listener_name,
						 source.listen(listener_name,
									   boost::bind(&LLEventPump::post, &dest,
												   _1)));
		}
		else
		{
			// "dest" unspecified means to direct events on "source" to our
			// plugin listener.
			saveListener(source_name, listener_name,
						 mConnect(source, listener_name));
		}
		reply["status"] = true;
	}
	catch (const LLEventPump::DupListenerName &)
	{
		// Pass: status already set to false.
	}
}

void LLLeapListener::stopListening(const LLSD& request)
{
	Response reply(LLSD(), request);

	std::string source_name = request["source"];
	std::string listener_name = request["listener"];

	listen_map_t::iterator it =
		mListeners.find(listen_map_t::key_type(source_name, listener_name));

	reply["status"] = false;
	if (it != mListeners.end())
	{
		reply["status"] = true;
		it->second.disconnect();
		mListeners.erase(it);
	}
}

void LLLeapListener::ping(const LLSD& request) const
{
	// Do nothing, default reply suffices
	Response(LLSD(), request);
}

void LLLeapListener::getAPIs(const LLSD& request) const
{
	Response reply(LLSD(), request);

	for (auto& ea : LLEventAPI::instance_snapshot())
	{
		LLSD info;
		info["desc"] = ea.getDesc();
		reply[ea.getName()] = info;
	}
}

void LLLeapListener::getAPI(const LLSD& request) const
{
	Response reply(LLSD(), request);

	auto found = LLEventAPI::getNamedInstance(request["api"]);
	if (found)
	{
		reply["name"] = found->getName();
		reply["desc"] = found->getDesc();
		reply["key"] = found->getDispatchKey();
		LLSD ops;
		for (LLEventAPI::const_iterator it = found->begin(),
										end = found->end();
			 it != end; ++it)
		{
			ops.append(found->getMetadata(it->first));
		}
		reply["ops"] = ops;
	}
}

void LLLeapListener::getFeatures(const LLSD& request) const
{
	// Merely constructing and destroying a Response object suffices here.
	// Giving it a name would only produce fatal 'unreferenced variable'
	// warnings.
	Response(getFeatures(), request);
}

void LLLeapListener::getFeature(const LLSD& request) const
{
	Response reply(LLSD(), request);

	LLSD::String feature_name(request["feature"]);
	LLSD features(getFeatures());
	if (features[feature_name].isDefined())
	{
		reply["feature"] = features[feature_name];
	}
}

void LLLeapListener::saveListener(const std::string& pump_name,
								  const std::string& listener_name,
								  const LLBoundListener& listener)
{
	mListeners.insert(
		listen_map_t::value_type(listen_map_t::key_type(pump_name,
													    listener_name),
								 listener));
}

///////////////////////////////////////////////////////////////////////////////
// LLLeap class and its LLLeapImpl
///////////////////////////////////////////////////////////////////////////////

static std::set<std::string> sKnownInterpreters =
{
#if LL_WINDOWS
	"pythonw3.exe", "pythonw.exe", "pyw.exe", "python.exe", "lua.exe", "cmd.exe"
#else
	"python3", "python", "python2", "lua"
#endif
};

class LLLeapImpl final : public LLLeap
{
	LOG_CLASS(LLLeapImpl);

public:
	// Called only by LLLeap::create()
	LLLeapImpl(const LLSD& params)
	:	mDonePump("LLLeap", true),
		mReplyPump(LLUUID::generateNewID().asString()),
		mExpect(0),
		mBinaryInput(false),
		mBinaryOutput(false),
		mListener(new LLLeapListener(boost::bind(&LLLeapImpl::connect, this,
												 _1, _2)))
	{
		// Rule out unpopulated params block
		if (!params.isMap() || !params.has("executable"))
		{
			throw Error("no plugin command");
		}

		if (params.has("desc"))
		{
			mDesc = params["desc"].asString();
		}
		// Do not leave desc empty either, but in this case, if we were not
		// given one, we will fake one.
		if (mDesc.empty())
		{
			mDesc = params["executable"].asString();
			// If we are running a script for a known interpreter, use the
			// script name for the desc instead of just the interpreter
			// executable name.
			if (params.has("args") && params["args"].isArray())
			{
				std::string desclower(mDesc);
				LLStringUtil::toLower(desclower);
				if (sKnownInterpreters.count(desclower))
				{
					mDesc = params["args"][0].asString();
				}
			}
		}

		// Listen for child "termination" right away to catch launch errors.
		mDonePump.listen("LLLeap", boost::bind(&LLLeapImpl::badLaunch, this,
						 _1));

		// Get a modifiable copy of params block to set files and postend.
		LLSD pparams(params);
		// Copy our deduced mDesc back into the params block
		pparams["desc"] = mDesc;
		// Pipe stdin, stdout and stderr
		pparams["files"] = llsd::array("pipe", "pipe", "pipe"),
		pparams["postend"] = mDonePump.getName();
		mChild = LLProcess::create(pparams);
		// If that did not work, no point in keeping this LLLeap object.
		if (!mChild)
		{
			throw Error("failed to run " + mDesc);
		}

		// Launch apparently worked. Change our mDonePump listener.
		mDonePump.stopListening("LLLeap");
		mDonePump.listen("LLLeap", boost::bind(&LLLeapImpl::done, this, _1));

		// Child might pump large volumes of data through either stdout or
		// stderr. Do not bother copying all that data into notification event.
		LLProcess::ReadPipe& childout(mChild->getReadPipe(LLProcess::STDOUT));
		childout.setLimit(20);
		LLProcess::ReadPipe& childerr(mChild->getReadPipe(LLProcess::STDERR));
		childerr.setLimit(20);

		// Serialize any event received on mReplyPump to our child's stdin.
		mStdinConnection = connect(mReplyPump, "LLLeap");

		// Listening on stdout is stateful. In general, we are either waiting
		// for the length prefix or waiting for the specified length of data.
		// We address that with two different listener methods, one of which
		// is blocked at any given time.
		mStdoutConnection =
			childout.getPump().listen("prefix",
									  boost::bind(&LLLeapImpl::rstdout, this,
												  _1));
		mStdoutDataConnection =
			childout.getPump().listen("data",
									  boost::bind(&LLLeapImpl::rstdoutData,
												  this));
		mBlocker.reset(new LLEventPump::Blocker(mStdoutDataConnection));

		// Log anything sent up through stderr. When a typical program
		// encounters an error, it writes its error message to stderr and
		// terminates with nonzero exit code. In particular, the Python
		// interpreter behaves that way. More generally, though, a plugin
		// author can log whatever s/he wants to the viewer log using stderr.
		mStderrConnection =
			childerr.getPump().listen("LLLeap",
									  boost::bind(&LLLeapImpl::rstderr, this,
												  _1));

		// Note: at this point, LL's implementation creates an LLError recorder
		// to divert llerrs and inform the child process through its stdin when
		// they occur: this is of no use to us since we only employ LLLeap with
		// external plugins (which cannot use LLError by themselves) and since
		// llerrs in the viewer code itself cause a voluntary crash anyway. I
		// suppose this was done by LL to cover unit tests code and plugins,
		// but we do not use any of these in the Cool VL Viewer. HB

		// Send child a preliminary event reporting our own reply-pump name,
		// which would otherwise be pretty tricky to guess !
		wstdin(mReplyPump.getName(),
			   LLSDMap("command", mListener->getName())
			   // Include LLLeap features: this may be important for child to
			   // construct (or recognize) current protocol.
			   ("features", LLLeapListener::getFeatures()));
	}

	// Normally we would expect to arrive here only via done()
	~LLLeapImpl() override
	{
		LL_DEBUGS("Leap") << "Destroying LLLeap(\"" << mDesc << "\")"
							<< LL_ENDL;
	}

	// Listener for failed launch attempt
	bool badLaunch(const LLSD& data)
	{
		llwarns << data["string"].asString() << llendl;
		return false;
	}

	// Listener for child-process termination
	bool done(const LLSD& data)
	{
		// Log the termination
		llinfos << data["string"].asString() << llendl;

		// Any leftover data at this moment are because protocol was not
		// satisfied. Possibly the child was interrupted in the middle of
		// sending a message, possibly the child did not flush stdout before
		// terminating, possibly it is just garbage. Log its existence but
		// discard it.
		LLProcess::ReadPipe& childout(mChild->getReadPipe(LLProcess::STDOUT));
		if (childout.size())
		{
			LLProcess::ReadPipe::size_type peeklen =
				llmin(LLProcess::ReadPipe::size_type(50), childout.size());
			llwarns << "Discarding final " << childout.size() << " bytes: "
					<< childout.peek(0, peeklen) << "..." << llendl;
		}

		// Kill this instance. MUST BE LAST before return !
		delete this;
		return false;
	}

	// Listener for events on mReplyPump: send to child stdin
	bool wstdin(const std::string& pump, const LLSD& data)
	{
		LLSD packet(LLSDMap("pump", pump)("data", data));

		std::ostringstream buffer;
		if (mBinaryOutput)
		{
			// SL-18330: for large data blocks, it is much faster to parse
			// binary LLSD than notation LLSD. Use serialize(LLSD_BINARY)
			// rather than directly calling LLSDBinaryFormatter because,
			// unlike the latter, serialize() prepends the relevant header,
			// needed by a general-purpose LLSD parser to distinguish binary
			// from notation.
			LLSDSerialize::serialize(packet, buffer,
									 LLSDSerialize::LLSD_BINARY,
									 LLSDFormatter::OPTIONS_NONE);
		}
		else
		{
			buffer << LLSDNotationStreamer(packet);
		}

		LL_DEBUGS("Leap") << "Sending: " << (U64)buffer.tellp() << ':';
		const std::streampos truncate = 80;
		if (buffer.tellp() <= truncate)
		{
			LL_CONT << buffer.str();
		}
		else
		{
			LL_CONT << buffer.str().substr(0, truncate) << "...";
		}
		LL_CONT << LL_ENDL;

		LLProcess::WritePipe& childin(mChild->getWritePipe(LLProcess::STDIN));
		childin.get_ostream() << (U64)buffer.tellp() << ':' << buffer.str()
							  << std::flush;
		return false;
	}

	// Initial state of stateful listening on child stdout: wait for a length
	// prefix, followed by ':'.
	bool rstdout(const LLSD& data)
	{
		LLProcess::ReadPipe& childout(mChild->getReadPipe(LLProcess::STDOUT));
		// It's possible we got notified of a couple digit characters without
		// seeing the ':' -- unlikely, but still. Until we see ':', keep
		// waiting.
		if (childout.contains(':'))
		{
			std::istream& childstream(childout.get_istream());
			// Saw ':', read length prefix and store in mExpect.
			size_t expect;
			childstream >> expect;
			int colon = childstream.get();
			if ((char)colon != ':')
			{
				// Protocol failure. Clear out the rest of the pending data in
				// childout (well, up to a max length) to log what was wrong.
				LLProcess::ReadPipe::size_type readlen =
					llmin(childout.size(), LLProcess::ReadPipe::size_type(80));
				badProtocol(llformat("%d%c%s", expect, colon,
									 childout.read(readlen).c_str()));
			}
			else
			{
				// Saw length prefix, saw colon, life is good. Now wait for
				// that length of data to arrive.
				mExpect = expect;
				LL_DEBUGS("Leap") << "Got length, waiting for "
								  << mExpect << " bytes of data" << LL_ENDL;
				// Block calls to this method; resetting mBlocker unblocks
				// calls to the other method.
				mBlocker.reset(new LLEventPump::Blocker(mStdoutConnection));
				// Go check if we have already received all the advertised
				// data.
				if (childout.size())
				{
					rstdoutData();
				}
			}
		}
		else if (childout.contains('\n'))
		{
			// Since this is the initial listening state, this is where we
			// would arrive if the child is not following protocol at all; say
			// because the user specified 'ls' or some darn thing.
			badProtocol(childout.getline());
		}
		return false;
	}

	// State in which we listen on stdout for the specified length of data to
	// arrive.
	bool rstdoutData()
	{
		LLProcess::ReadPipe& childout(mChild->getReadPipe(LLProcess::STDOUT));
		// Until we have accumulated the promised length of data, keep waiting.
		if (childout.size() >= mExpect)
		{
			// Ready to rock and roll.
			LLSD data;
			bool success;
#if LL_USE_NEW_DESERIALIZE
			if (mBinaryInput)
			{
				// SL-18330: accept any valid LLSD serialization format from
				// child. Unfortunately this runs into trouble we have not yet
				// debugged.
				success = LLSDSerialize::deserialize(data,
													 childout.get_istream(),
													 mExpect);
			}
			else
#endif
			{
				// Specifically require notation LLSD from child.
				LLPointer<LLSDParser> parser(new LLSDNotationParser());
				S32 count = parser->parse(childout.get_istream(), data,
										  mExpect);
				success = count != LLSDParser::PARSE_FAILURE;
			}
			if (!success)
			{
				badProtocol("unparseable LLSD data");
			}
			else if (!(data.isMap() && data["pump"].isString() &&
					 data.has("data")))
			{
				// We got an LLSD object, but it lacks required keys
				badProtocol("missing 'pump' or 'data'");
			}
			else
			{
				// The LLSD object we got from our stream contains the keys we
				// need.
				gEventPumps.obtain(data["pump"]).post(data["data"]);
				// Block calls to this method; resetting mBlocker unblocks calls
				// to the other method.
				mBlocker.reset(new LLEventPump::Blocker(mStdoutDataConnection));
				// Go check for any more pending events in the buffer.
				if (childout.size())
				{
					LLSD updata(data);
					data["len"] = LLSD::Integer(childout.size());
					rstdout(updata);
				}
			}
		}
		return false;
	}

	void badProtocol(const std::string& data)
	{
		llwarns << mDesc << ": invalid protocol: " << data << llendl;
		// No point in continuing to run this child.
		mChild->kill();
	}

	// Listen on child stderr and log everything that arrives
	bool rstderr(const LLSD& data)
	{
		LLProcess::ReadPipe& childerr(mChild->getReadPipe(LLProcess::STDERR));
		// We might have gotten a notification involving only a partial line
		// or multiple lines. Read all complete lines; stop when there is only
		// a partial line left.
		while (childerr.contains('\n'))
		{
			// DO NOT make calls with side effects in a logging statement !  If
			// that log level is suppressed, your side effects WOULD NOT
			// HAPPEN.
			std::string line(childerr.getline());
			// Log the received line. Prefix it with the desc so we know which
			// plugin it is from. This method name rstderr() is intentionally
			// chosen to further qualify the log output.
			llinfos << mDesc << ": " << line << llendl;
		}
		// What if child writes a final partial line to stderr ?
		if (data["eof"].asBoolean() && childerr.size())
		{
			std::string rest(childerr.read(childerr.size()));
			// Read all remaining bytes and log.
			llinfos << mDesc << ": " << rest << llendl;
		}
		return false;
	}

	// To toggle binary LLSD stream from the viewer to the LEAP plugin
	LL_INLINE void enableBinaryOutput(bool enable) override
	{
		mBinaryOutput = enable;
	}

	// To toggle binary LLSD stream from the LEAP plugin to the viewer (broken)
	LL_INLINE void enableBinaryInput(bool enable) override
	{	
		mBinaryInput = enable;
	}

	// Introspection methods overrides. HB

	LL_INLINE bool binaryOutputEnabled() const override
	{
		return mBinaryOutput;
	}

	LL_INLINE bool binaryInputEnabled() const override
	{
		return mBinaryInput;
	}

	LL_INLINE const std::string& getDesc() const override
	{
		return mDesc;
	}

	LL_INLINE const std::string& getProcDesc() const override
	{
		return mChild ? mChild->getDesc() : LLStringUtil::null;
	}

	LL_INLINE const std::string& getExecutable() const override
	{
		return mChild ? mChild->getExecutable() : LLStringUtil::null;
	}

	LL_INLINE const std::string& getInterpreter() const override
	{
		return mChild ? mChild->getInterpreter() : LLStringUtil::null;
	}

	LL_INLINE const std::string& getCwd() const override
	{
		return mChild ? mChild->getCwd() : LLStringUtil::null;
	}

	// Not inlined to avoid scattering empty_vec's everywhere in the code...
	LL_NO_INLINE const std::vector<std::string>& getArgs() const override
	{
		static const std::vector<std::string> empty_vec;
		return mChild ? mChild->getArgs() : empty_vec;
	}

private:
	// We always want to listen on mReplyPump with wstdin(); under some
	// circumstances we will also echo other LLEventPumps to the plugin.
	LLBoundListener connect(LLEventPump& pump, const std::string& listener)
	{
		// Serialize any event received on the specified LLEventPump to our
		// child's stdin, suitably enriched with the pump name on which it was
		// received.
		return pump.listen(listener,
						   boost::bind(&LLLeapImpl::wstdin, this,
									   pump.getName(), _1));
	}

private:
	std::string								mDesc;
	LLProcess::ptr_t						mChild;
	LLEventStream							mDonePump;
	LLEventStream							mReplyPump;
	LLTempBoundListener						mStdinConnection;
	LLTempBoundListener						mStdoutConnection;
	LLTempBoundListener						mStdoutDataConnection;
	LLTempBoundListener						mStderrConnection;
	boost::scoped_ptr<LLEventPump::Blocker>	mBlocker;
	boost::scoped_ptr<LLLeapListener>		mListener;
	LLProcess::ReadPipe::size_type			mExpect;
	bool									mBinaryInput;
	bool									mBinaryOutput;
};

// These must follow the declaration of LLLeapImpl, so they may as well be
// last.
LLLeap* LLLeap::create(const LLSD& params, bool exc)
{
	// If caller is willing to permit exceptions, just instantiate.
	if (exc)
	{
		return new LLLeapImpl(params);
	}

	// Caller insists on suppressing LLLeap::Error. Very well, catch it.
	try
	{
		return new LLLeapImpl(params);
	}
	catch (const LLLeap::Error&)
	{
		return NULL;
	}
}

LLLeap* LLLeap::create(const std::string& desc,
					   const std::vector<std::string>& plugin, bool exc)
{
	LLSD params = LLSD::emptyMap();
	params["desc"] = desc;
	U32 count = plugin.size();
	if (count)
	{
		params["executable"] = plugin[0];
		if (count > 1)
		{
			LLSD args = LLSD::emptyArray();
			for (U32 i = 1; i < count; ++i)
			{
				args.set(LLSD::Integer(i - 1), LLSD(plugin[i]));
			}
			params["args"] = args;
		}
	}
	return create(params, exc);
}

LLLeap* LLLeap::create(const std::string& desc, const std::string& plugin,
					   bool exc)
{
	// Use LLStringUtil::getTokens() to parse the command line
	return create(desc,
				  LLStringUtil::getTokens(plugin,
										  " \t\r\n",	// drop_delims
										  "",			// No keep_delims
										  "\"'",		// Valid quotes
										  "\\"),		// Backslash escape
				  exc);
}
