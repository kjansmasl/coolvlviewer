/**
 * @file llappviewerlinux.cpp
 * @brief The LLAppViewerLinux class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * Copyright (c) 2013-2022, Henri Beauchamp (better generateSerialNumber(),
 * new ELFIO crashlogger with 64 bits support, glib-dbus implementation,
 * Lua via DBus, Vulkan detection, etc).
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <execinfo.h>			// Backtrace with glibc
#include <exception>
#include <fstream>
#include <sstream>

// JsonCpp include (for the Vulkan API version extraction)
#include "reader.h"

#include "glib.h"
#include "glib-object.h"
#include "gio/gio.h"

#ifdef __GNUC__
# include <cxxabi.h>			// For symbol demangling
#endif
#include "elfio/elfio.hpp"		// For better backtraces

#include "llappviewerlinux.h"

#include "llapp.h"
#include "llcommandlineparser.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfindlocale.h"
#include "llmd5.h"
#include "llwindowsdl.h"

#include "llgridmanager.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"

// Note: LL_CALL_SLURL_DISPATCHER_IN_CALLBACK is defined in llappviewer.h
#if LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
# include "llurldispatcher.h"
#else
std::string LLAppViewerLinux::sReceivedSLURL = "";
#endif

extern "C" {	// Do not mangle the name for the main() function !
int main(int argc, char** argv);
}

// Used for glib events pumping
// 5 checks a second *should* be more than enough
constexpr F32 GLIB_EVENTS_THROTTLE = 0.2f;
// Pumping shall not eat more than that...
constexpr F32 GLIB_PUMP_TIMEOUT = 0.01f;
constexpr F32 GLIB_PUMP_RETRY_AFTER = 0.01f;
static LLTimer sPumpTimer;

static int sArgC = 0;
static char** sArgV = NULL;
// Used for demangling function names during stack trace logging. HB
static size_t sDemangleBufferSize = 1024;
static char* sDemangleBuffer = NULL;

static void (*sOldTerminateHandler)() = NULL;

static void exceptionTerminateHandler()
{
	// Reinstall default terminate() handler in case we re-terminate.
	if (sOldTerminateHandler)
	{
		std::set_terminate(sOldTerminateHandler);
	}
	// Treat this like a regular viewer crash, with nice stacktrace etc.
	LLAppViewer::handleSyncViewerCrash();
	LLAppViewer::handleViewerCrash();
	// We have probably been killed-off before now, but...
	sOldTerminateHandler();	// Call old terminate() handler
}

int main(int argc, char** argv)
{
	sArgC = argc;
	sArgV = argv;

	// Reserve some space for stack trace function names demangling in case of a
	// crash; also, reserve enough such space that we do not risk seeing
	// abi::__cxa_demangle() reallocating it. HB
	sDemangleBuffer = (char*)malloc(sDemangleBufferSize);

	LLAppViewer* viewer_app_ptr = new LLAppViewerLinux();

	// Install unexpected exception handler
	sOldTerminateHandler = std::set_terminate(exceptionTerminateHandler);
	// Install crash handlers
	LLApp::setErrorHandler(LLAppViewer::handleViewerCrash);
	LLApp::setSyncErrorHandler(LLAppViewer::handleSyncViewerCrash);

	LLApp::InitState state = viewer_app_ptr->init();
	if (state != LLApp::INIT_OK)
	{
		if (state != LLApp::INIT_OK_EXIT)
		{
			llwarns << "Application init failed." << llendl;
			return LLAppViewer::EXIT_INIT_FAILED;
		}
		return LLAppViewer::EXIT_OK;	// No error, just exiting immediately.
	}

	llinfos << "Compiled against glib v" << GLIB_MAJOR_VERSION << "."
			<< GLIB_MINOR_VERSION << "." << GLIB_MICRO_VERSION
			<< " - Running against glib v" << glib_major_version << "."
			<< glib_minor_version << "." << glib_micro_version << llendl;
	if (glib_minor_version < GLIB_MINOR_VERSION ||
		(glib_minor_version == GLIB_MINOR_VERSION &&
		 glib_micro_version < GLIB_MICRO_VERSION))
	{
		llwarns << "System glib version too old, expect problems !" << llendl;
	}

	// Initialize our pump timer
	sPumpTimer.reset();
	sPumpTimer.setTimerExpirySec(GLIB_PUMP_TIMEOUT);

	// Run the application main loop
	if (!LLApp::isQuitting())
	{
		viewer_app_ptr->mainLoop();
	}

	// We do not want to do cleanup here if the error handler got called: the
	// assumption is that the error handler is responsible for doing app
	// cleanup if there was a problem.
	if (!LLApp::isError())
	{
		viewer_app_ptr->cleanup();
	}

	delete viewer_app_ptr;
	viewer_app_ptr = NULL;

	// Free the demangle buffer. HB
	free(sDemangleBuffer);

	return gExitCode;
}

#define MAX_STACK_TRACE_DEPTH 40
// This uses glibc's basic built-in stack-trace functions for a not very
// amazing backtrace.
static LL_INLINE bool do_basic_glibc_backtrace()
{
	void* stackarray[MAX_STACK_TRACE_DEPTH];
	size_t size;
	char** strings;
	size_t i;
	bool success = false;

	size = backtrace(stackarray, MAX_STACK_TRACE_DEPTH);
	strings = backtrace_symbols(stackarray, size);

	std::string strace_filename =
		gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "stack_trace.log");
	llinfos << "Opening stack trace file " << strace_filename << llendl;
	LLFILE* strace_fp = LLFile::open(strace_filename, "w");
	if (!strace_fp)
	{
		llinfos << "Opening stack trace file " << strace_filename
				<< " failed. Using stderr." << llendl;
		strace_fp = stderr;
	}

	if (size)
	{
		for (i = 0; i < size; ++i)
		{
			// The format of the strace_fp is very specific, to allow (kludgy)
			// machine-parsing
			fprintf(strace_fp, "%-3lu ", (unsigned long)i);
			fprintf(strace_fp, "%-32s\t", "unknown");
			fprintf(strace_fp, "%p ", stackarray[i]);
			fprintf(strace_fp, "%s\n", strings[i]);
		}

		success = true;
	}

	if (strace_fp != stderr)
	{
		LLFile::close(strace_fp);
	}

	free(strings);
	return success;
}

// This uses glibc's basic built-in stack-trace functions together with ELFIO's
// ability to parse the .symtab ELF section for better symbol extraction
// without exporting symbols (which would cause subtle, fatal bugs).
static LL_INLINE bool do_elfio_glibc_backtrace()
{
	std::string app_filename = gDirUtilp->getExecutablePathAndName();

	std::string strace_filename =
		gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "stack_trace.log");
	llinfos << "Opening stack trace file " << strace_filename << llendl;
	LLFILE* strace_fp = LLFile::open(strace_filename, "w");
	if (!strace_fp)
	{
		llinfos << "Opening stack trace file " << strace_filename
				<< " failed. Using stderr." << llendl;
		strace_fp = stderr;
	}

	// Get backtrace address list and basic symbol info
	void* stackarray[MAX_STACK_TRACE_DEPTH];
	size_t btsize = backtrace(stackarray, MAX_STACK_TRACE_DEPTH);
	char** strings = backtrace_symbols(stackarray, btsize);

	// Create an ELF reader for our app binary
	ELFIO::elfio reader;
	ELFIO::Elf_Half sections_count = 0;
	if (reader.load(app_filename.c_str()))
	{
		sections_count = reader.sections.size();
	}
	if (!sections_count)
	{
		// Failed to open our binary and read its symbol table somehow
		llinfos << "Could not initialize ELF symbol reading - doing basic backtrace."
				<< llendl;
		if (strace_fp != stderr)
		{
			LLFile::close(strace_fp);
		}
		// Note that we may be leaking some of the above ELFIO objects now,
		// but it is expected that we will be dead soon and we want to tread
		// delicately until we get *some* kind of useful backtrace.
		return do_basic_glibc_backtrace();
	}

	// Iterate over trace and symtab, looking for plausible symbols
	std::string name;
	ELFIO::Elf64_Addr value = 0;
	ELFIO::Elf_Xword ssize = 0;
	ELFIO::Elf_Half section = 0;
	unsigned char bind = 0;
	unsigned char type = 0;
	unsigned char other = 0;
	int demangle_result = 0;
	for (size_t btpos = 0; btpos < btsize; ++btpos)
	{
		uintptr_t addr = uintptr_t(stackarray[btpos]);
#if 1	// See 'Update' below.
		// backtrace() got it all wrong with clang-compiled binaries, and
		// returns an absolute address instead of an offset to the start of the
		// program binary (which we need to compare with ELFIO's symbols
		// addresses). Thankfully, we can get the latter from the strings array
		// returned by backtrace_symbols() instead... HB
		// Update (2023-10-19): also seen happening for some stack traces on
		// systems running a different glibc than the one the gcc-compiled
		// viewer was built against.. So, let's always use this workaround. HB
		name = strings[btpos];
		size_t idx = name.find("(+0x");
		if (idx != std::string::npos)
		{
			name.erase(0, idx + 2);
			idx = name.find(")");
			if (idx != std::string::npos)
			{
				name.erase(idx);
				unsigned long a = 0;
				if (!name.empty() && sscanf(name.c_str(), "%lx", &a) == 1)
				{
					addr = (uintptr_t)a;
				}
			}
		}
#endif
		// The format of the strace_fp is very specific, to allow (kludgy)
		// machine-parsing
		fprintf(strace_fp, "%-3ld ", (long)btpos);
		bool found = false;
		for (int i = 0; i < sections_count && !found; ++i)
		{
			ELFIO::section* psec = reader.sections[i];
			if (!psec || psec->get_type() != ELFIO::SHT_SYMTAB)
			{
				continue;
			}
			const ELFIO::symbol_section_accessor symbols(reader, psec);
			for (unsigned int j = 0, count = symbols.get_symbols_num();
				 j < count; ++j)
			{
				symbols.get_symbol(j, name, value, ssize, bind, type, section,
								   other);
				// Check if trace address is within this symbol range
				if (addr >= uintptr_t(value) &&
				    addr < uintptr_t(value + ssize))
				{
					// Symbol is inside viewer code
					fprintf(strace_fp, "com.secondlife.indra.viewer\t%p ",
							(void*)addr);

					char* demangled = abi::__cxa_demangle(name.c_str(),
														  sDemangleBuffer,
														  &sDemangleBufferSize,
														  &demangle_result);
					if (demangled)
					{
						fprintf(strace_fp, "%s", demangled);
					}
					else // Failed demangle; print it raw
					{
						fprintf(strace_fp, "%s", name.c_str());
					}
					// Print offset from symbol start
					fprintf(strace_fp, " + %lu\n", addr - value);
					found = true;
					break;
				}
			}
		}
		if (!found)
		{
			// Fallback: did not find a suitable symbol in the binary; it is
			// probably a symbol in a DSO; use glibc's idea of what it should
			// be.
			fprintf(strace_fp, "unknown\t%p ", (void*)addr);
			fprintf(strace_fp, "%s\n", strings[btpos]);
		}
	}

	if (strace_fp != stderr)
	{
		LLFile::close(strace_fp);
	}

	free(strings);

	llinfos << "Finished generating stack trace." << llendl;

	return true;
}

void LLAppViewerLinux::handleSyncCrashTrace()
{
	// Free our reserved memory space before dumping the stack trace (it should
	// already be freed at this point, but it does not hurt calling this
	// function twice).
	LLMemory::cleanupClass();

	// This backtrace writes into stack_trace.log
	do_elfio_glibc_backtrace();
}

bool LLAppViewerLinux::beingDebugged()
{
	static enum { unknown, no, yes } debugged = unknown;

	if (debugged == unknown)
	{
		debugged = no;

		// Note that the debugger is the parent process of the viewer. HB
		LLFILE* fp = LLFile::open(llformat("/proc/%d/cmdline", getppid()),
								  "r");
		if (fp)
		{
			char buf[256];
			if (fgets(buf, sizeof(buf) - 1, fp))
			{
				std::string cmdline(buf);
				if (cmdline.find("gdb") != std::string::npos ||
					cmdline.find("edb") != std::string::npos ||
					cmdline.find("lldb") != std::string::npos)
				{
					debugged = yes;
				}
			}
			LLFile::close(fp);
		}
	}

	bool debug = LLError::Log::sIsBeingDebugged = debugged == yes;
	return debug;
}

void LLAppViewerLinux::initLogging()
{
	// Remove the last stack trace, if any
	std::string old_stack_file =
		gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "stack_trace.log");
	LLFile::remove(old_stack_file);

	LLAppViewer::initLogging();
}

bool LLAppViewerLinux::initParseCommandLine(LLCommandLineParser& clp)
{
	if (!clp.parseCommandLine(sArgC, sArgV))
	{
		return false;
	}

	// Find the system language.
	FL_Locale* locale = NULL;
	FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
	if (success >= FL_CONFIDENT && locale->lang)
	{
		llinfos << "Language " << ll_safe_string(locale->lang) << llendl;
		llinfos << "Location " << ll_safe_string(locale->country) << llendl;
		llinfos << "Variant " << ll_safe_string(locale->variant) << llendl;

		LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
		if (c)
		{
			c->setValue(std::string(locale->lang), false);
		}
	}
	FL_FreeLocale(&locale);

	return true;
}

// Takes the longest scsi-*, ata-* or nvme-* entry in /dev/disk/by-id and
// hashes it into a MD5 sum (such entries correspond to physical disks and
// contain the drive serial number).
// This is a much better algorithm than LL's, since the latter takes only the
// longest id in /dev/disk/by-uuid/ which depends on the *currently* mounted
// disks, the resulting derived serial number not being unique for a given
// Linux system (e.g. if the user mounts an USB stick, the serial number may
// change). HB
std::string LLAppViewerLinux::generateSerialNumber()
{
	std::string best;
	const std::string iddir = "/dev/disk/by-id/";
	if (LLFile::isdir(iddir))
	{
		std::string first, link_name;
		bool best_is_nvme = false;
		LLDirIterator iter(iddir);
		while (iter.next(link_name))
		{
			LLStringUtil::toLower(link_name);
			if (first.empty())
			{
				// Remember the first available entry in case nothing better
				// can be found later...
				first = link_name;
			}
			if (link_name.rfind("-part") != std::string::npos)
			{
				// Skip partition IDs: we keep only the drives.
				continue;
			}
			bool is_nvme = link_name.find("nvme-", 0, 5) == 0;
			if (!is_nvme && link_name.find("ata-") != 0 &&
				link_name.find("scsi-") != 0)
			{
				// Skip anything not connected to an ATA, SCSI or NVME port
				// (since we do not want to take removable devices into
				// account).
				continue;
			}
			// If the best found Id so far corresponds to an NVME disk and this
			// new found Id pertains to an ATA or SCSI disk, then use it
			// instead. This is for compatibility reasons with former viewer
			// versions that did not use NVME disk serials; the latter are now
			// taken into account, but only when no ATA and no SCSI drive exist
			// in the system.
			if (best_is_nvme && !is_nvme)
			{
				best = link_name;
				best_is_nvme = false;
				continue;
			}
			// If nothing was found so far, unconditionally adopt this Id as
			// the best one.
		    if (best.empty())
			{
				best = link_name;
				best_is_nvme = is_nvme;
				continue;
			}
			// If we already have a best Id corresponding to an ATA or SCSI
			// disk and this new found Id is an NVME disk, then keep our
			// current best Id.
			if (is_nvme && !best_is_nvme)
			{
				continue;
			}
			// If the best Id so far is longer than the new found one, keep the
			// former.
			if (link_name.length() < best.length())
			{
				continue;
			}
			// If the new found Id comes after the best one in alphabetical
			// order, then adopt it as the best one.
		    if (link_name > best)
			{
				best = link_name;
				best_is_nvme = is_nvme;
			}
		}
		if (best.empty())
		{
			best = first;
		}
		LL_DEBUGS("AppInit") << "Using disk Id: " << best << LL_ENDL;
	}
	// Fallback to machine-id, which is "less unique" since it is a per-Linux
	// installation Id and the same PC could run several...
	const char* id_file = "/etc/machine-id";
	if (best.empty() && LLFile::isfile(id_file))
	{
		std::ifstream ifs(id_file);
		if (ifs.good())
		{
			std::stringstream buffer;
			buffer << ifs.rdbuf();
			best = buffer.str();
			llinfos << "Could not find any disk Id: using /etc/machine-id."
					<< llendl;
		}
	}
	if (best.empty()) // This should never happen in any modern Linux system...
	{
		llwarns << "Could not find any machine Id: using a random Id."
				<< llendl;
		// Totally random and regenerated at each viewer session...
		LLUUID id;
		id.generate();
		best = id.asString();
	}

	// We do not return the disk Id itself, but a hash of it
	LLMD5 md5(reinterpret_cast<const unsigned char*>(best.c_str()));
	char serial_md5[MD5HEX_STR_SIZE];
	md5.hex_digest(serial_md5);
	return std::string(serial_md5);
}

// This code was called from llappviewer.cpp via a virtual LLWindow method
// (processMiscNativeEvents()) which was only actually implemented under Linux
// via LLWindowSDL::processMiscNativeEvents(), so there was strictly no point
// in using such a virtual method to cover all OSes !
// Moving this code here also got the nice effect to remove the dependency on
// glib for the llwindow library; *all* glib-related code is now held in this
// file only !  HB
//static
void LLAppViewerLinux::pumpGlib()
{
	if (sPumpTimer.hasExpired())
	{
		// Pump until we have nothing left to do or passed GLIB_PUMP_TIMEOUT
		// of a second pumping.
		sPumpTimer.reset();
		sPumpTimer.setTimerExpirySec(GLIB_PUMP_TIMEOUT);
		while (g_main_context_pending(NULL))
		{
			g_main_context_iteration(NULL, FALSE);
			if (sPumpTimer.hasExpired())
			{
				llwarns_sparse << "Reached GLIB_PUMP_TIMEOUT: something is spamming us !"
							   << llendl;
				// Continue pumping in a subsequent (but close) frame...
				sPumpTimer.reset();
				sPumpTimer.setTimerExpirySec(GLIB_PUMP_RETRY_AFTER);
				return;
			}
		}
		// Throttle to 1/GLIB_EVENTS_THROTTLE per second the number of loops,
		// as long as we could process all pending events in this loop.
		sPumpTimer.reset();
		sPumpTimer.setTimerExpirySec(GLIB_EVENTS_THROTTLE);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Vulkan detection used by llviewerstats.cpp
///////////////////////////////////////////////////////////////////////////////

//virtual
bool LLAppViewerLinux::probeVulkan(std::string& version)
{
	static std::string vk_api_version;
	static S32 has_vulkan = -1;	// -1 = not yet probed
	if (has_vulkan == -1)
	{
		has_vulkan = 0;	// Default to no Vulkan support
		// Probe for Vulkan capability (Henri Beauchamp 05/2020)
		// Check for presense of a Vulkan ICD-loader manifest file for a
		// Vulkan-capable GPU. Gives a good approximation of Vulkan capability
		// within current user systems from this.
		std::string fname;
		if (getenv("VK_ICD_FILENAMES"))
		{
			fname.assign(getenv("VK_ICD_FILENAMES"));
			size_t pos = fname.find(';');
			if (pos > 1)
			{
				// Only check for the first file if several are listed
				// (unlikely)...
				fname = fname.substr(0, pos);
			}
			if (LLFile::isfile(fname))
			{
				llinfos << "Found user-specified Vulkan ICD-loader manifest: "
						<< fname << llendl;
				has_vulkan = 1;
			}
		}
		if (has_vulkan == 0 &&
			(gGLManager.mIsNVIDIA || gGLManager.mIsAMD || gGLManager.mIsIntel))
		{
			std::vector<std::string> paths;
			paths.emplace_back("/etc/vulkan/icd.d/");
			paths.emplace_back("/usr/share/vulkan/icd.d/");
			paths.emplace_back("/usr/local/etc/vulkan/icd.d/");
			paths.emplace_back("/usr/local/share/vulkan/icd.d/");
			if (getenv("HOME"))
			{
				fname.assign(getenv("HOME"));
				paths.emplace_back(fname + "/.local/share/vulkan/icd.d/");
			}
			
			std::string icd_file;
			if (gGLManager.mIsNVIDIA)
			{
				icd_file = "nvidia_icd.json";
			}
			else if (gGLManager.mIsAMD)
			{
				icd_file = "radeon_icd.x86_64.json";
			}
			else	// Intel
			{
				icd_file = "intel_icd.x86_64.json";
			}
			for (S32 i = 0, count = paths.size(); i < count; ++i)
			{
				fname = paths[i] + icd_file;
				if (LLFile::isfile(fname))
				{
					llinfos << "Found matching Vulkan ICD-loader manifest: "
							<< fname << llendl;
					has_vulkan = 1;
					break;
				}
			}
		}
		// Get the Vulkan API version (Henri Beauchamp 01/2022)
		if (has_vulkan == 1)
		{
			std::ifstream file;
			file.open(fname.c_str(), std::ios::in);
			if (file.is_open())
			{
				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(file, root))
				{
					llwarns << "Cannot read Vulkan manifest file: " << fname
							<< llendl;
					has_vulkan = 0;
				}
				file.close();
				if (root.isMember("ICD"))
				{
					Json::Value icd = root["ICD"];
					for (auto it = icd.begin(), end = icd.end(); it != end;
						 ++it)
					{
						if (it.name() == "api_version")
						{
							vk_api_version = it->asString();
							llinfos << "Vulkan API version is: "
									<< vk_api_version << llendl;
							break;
						}
						// *TODO: also check the validity of "library_path" ?
					}
				}
				else
				{	
					llwarns << "Malformed Vulkan manifest file: " << fname
							<< llendl;
					has_vulkan = 0;
				}
			}
			else
			{	
				llwarns << "Could not open Vulkan manifest file: " << fname
						<< llendl;
				has_vulkan = 0;
			}
		}
	}
	version = vk_api_version;
	return has_vulkan == 1;
}

///////////////////////////////////////////////////////////////////////////////
// DBus support for SLURLs passing between viewer instances, and Lua via DBus
///////////////////////////////////////////////////////////////////////////////

#define VIEWERAPI_SERVICE "com.secondlife.ViewerAppAPIService"
#define VIEWERAPI_PATH "/com/secondlife/ViewerAppAPI"
#define VIEWERAPI_INTERFACE "com.secondlife.ViewerAppAPI"
#define VIEWERAPI_GOSURL_METHOD "GoSLURL"
#define VIEWERAPI_LUA_METHOD "LuaExec"

static GDBusNodeInfo* sIntrospectionData = NULL;
static guint sServerBusId = 0;

//virtual
bool LLAppViewerLinux::sendURLToOtherInstance(const std::string& url)
{
	bool success = false;

	GError* error = NULL;
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (bus)
	{
		if (error)
		{
			g_error_free(error);
			error = NULL;
		}
		llinfos << "Calling out another instance to send SLURL: " << url
				<< llendl;

		GDBusProxy* proxy =
			g_dbus_proxy_new_sync(bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
								  VIEWERAPI_SERVICE, VIEWERAPI_PATH,
								  VIEWERAPI_INTERFACE, NULL, &error);
		if (proxy)
		{
			if (error)
			{
				g_error_free(error);
				error = NULL;
			}
			GVariant* var = g_dbus_proxy_call_sync(proxy,
												   VIEWERAPI_GOSURL_METHOD,
												   g_variant_new("(s)",
																 url.c_str()),
												   G_DBUS_CALL_FLAGS_NONE,
												   -1, NULL, &error);
#if 0		// "Recent" (post v0.92) dbus-glib versions got a server-side bug
			// causing a timeout while the message was successfully passed...
			// Since we must deal with viewers still using dbus-glib and acting
			// as the server, we cannot avoid this limitation.
			if (var)
			{
				llinfos << "Call-out to other instance succeeded." << llendl;
				gboolean* rtn;
				g_variant_get(var, "(b)", &rtn);
				if (rtn)
				{
					success = (bool)*rtn;
					g_free(rtn);
					llinfos << "Returned boolean: "
							<< (success ? "TRUE" : "FALSE") << llendl;
				}
			}
			else
			{
				llinfos << "Call-out to other instance failed." << llendl;
			}
#else		// Just consider it to always be a success.
			success = true;
#endif
			if (var)
			{
				g_variant_unref(var);
			}
		}
		else
		{
			llinfos << "Call-out to other instance failed." << llendl;
		}

		g_object_unref(G_OBJECT(proxy));
	}
	else
	{
		llwarns << "Could not connect to session bus." << llendl;
	}

	if (error)
	{
		llinfos << "Completion message: " << error->message << llendl;
		g_error_free(error);
	}

	return success;
}

static void handle_method_call(GDBusConnection* connection,
							   const gchar* sender,
							   const gchar* object_path,
							   const gchar* interface_name,
							   const gchar* method_name,
							   GVariant* parameters,
							   GDBusMethodInvocation* invocation,
							   gpointer user_data)
{
	if (g_strcmp0(method_name, VIEWERAPI_GOSURL_METHOD) == 0)
	{
		const gchar* slurl;
		g_variant_get(parameters, "(&s)", &slurl);
		llinfos << "Was asked to go to slurl: " << slurl << llendl;

#if LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
		std::string url = slurl;
		LLMediaCtrl* web = NULL;
		LLURLDispatcher::dispatch(url, "clicked", web, false);
#else
		LLAppViewerLinux::setReceivedSLURL(slurl);
#endif

		// Always return a success; if the running viewer instance does not
		// know how to dispatch the passed SLURL, the sending instance won't
		// know either, so it is pointless to let it try and auto-login in a
		// place bearing an invalid SLURL, especially since it would disconnect
		// the running instance (auto-login reusing the last logged in avatar
		// credentials).
		gboolean ret = TRUE;
		g_dbus_method_invocation_return_value(invocation,
											  g_variant_new("(b)", &ret));
	}
	else if (g_strcmp0(method_name, VIEWERAPI_LUA_METHOD) == 0)
	{
		const gchar* cmdline;
		g_variant_get(parameters, "(&s)", &cmdline);
		if (gSavedSettings.getBool("LuaAcceptDbusCommands"))
		{
			llinfos << "Was asked to go execute Lua command line: " << cmdline
					<< llendl;
			// Note: we pass fake object Id and name to apply the same
			// restrictions as for Lua commands sent to the viewer via a
			// scripted object.
			if (HBViewerAutomation::sLuaDBusFakeObjectId.isNull())
			{
				HBViewerAutomation::sLuaDBusFakeObjectId.generate();
			}
			std::string ret =
				HBViewerAutomation::eval(std::string(cmdline), true,
										 HBViewerAutomation::sLuaDBusFakeObjectId,
										 "Lua D-Bus");
			llinfos << "Result: " << ret << llendl;
			g_dbus_method_invocation_return_value(invocation,
												  g_variant_new("(s)",
																ret.c_str()));
		}
		else
		{
			llwarns << "Rejected D-Bus Lua command: " << cmdline << llendl;
			g_dbus_method_invocation_return_value(invocation,
												  g_variant_new("(s)",
																"forbidden"));
		}
	}
	else
	{
		llwarns_once << "Rejected unknown method: " << method_name << llendl;
		g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
											  G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method name: %s",
											  method_name);
	}
}

static GVariant* handle_get_property(GDBusConnection* connection,
									 const gchar* sender,
									 const gchar* object_path,
									 const gchar* interface_name,
									 const gchar* property_name,
									 GError** error, gpointer user_data)
{
	g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
				"Getting property %s is not supported", property_name);
	return NULL;
}

static gboolean handle_set_property(GDBusConnection* connection,
									const gchar* sender,
									const gchar* object_path,
									const gchar* interface_name,
									const gchar* property_name,
									GVariant* value,
									GError** error, gpointer user_data)
{
	g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
				"Setting property %s is not supported", property_name);
	return false;
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
	handle_get_property,
	handle_set_property
};

static void on_bus_acquired(GDBusConnection* connection, const gchar* name,
							gpointer user_data)
{
	llinfos << "Acquired the bus: " << name << llendl;
	GError* error = NULL;
	guint id =
		g_dbus_connection_register_object(connection, VIEWERAPI_PATH,
										  sIntrospectionData->interfaces[0],
										  &interface_vtable, NULL, NULL,
										  &error);
	if (id <= 0)
	{
		llwarns << "Unable to register object: "
				<< (error ? error->message : "unknown reason") << llendl;
		if (sServerBusId)
		{
			llinfos << "Unowning the bus." << llendl;
			g_bus_unown_name(sServerBusId);
			sServerBusId = 0;
		}
	}
	if (error)
	{
		g_error_free(error);
	}
}

// Connect to the default DBUS, register our service/API.
//virtual
bool LLAppViewerLinux::initAppMessagesHandler()
{
	if (!sIntrospectionData)
	{
		const gchar introspection_xml[] =
			"<node name='" VIEWERAPI_PATH "'>"
			"  <interface name='" VIEWERAPI_INTERFACE "'>"
			"    <method name='" VIEWERAPI_GOSURL_METHOD "'>"
			"      <arg type='s' name='slurl' direction='in'/>"
			"      <arg type='b' name='success_ret' direction='out'/>"
			"    </method>"
			"    <method name='" VIEWERAPI_LUA_METHOD "'>"
			"      <arg type='s' name='cmdline' direction='in'/>"
			"      <arg type='s' name='result' direction='out'/>"
			"    </method>"
			"  </interface>"
			"</node>";
		sIntrospectionData = g_dbus_node_info_new_for_xml(introspection_xml,
														  NULL);
		if (!sIntrospectionData)
		{
			llwarns << "Failed to create instrospection data. Aborted."
					<< llendl;
			return false;
		}
	}
	if (sServerBusId <= 0)
	{
		sServerBusId = g_bus_own_name(G_BUS_TYPE_SESSION, VIEWERAPI_SERVICE,
									  G_BUS_NAME_OWNER_FLAGS_NONE,
									  on_bus_acquired,
									  NULL, NULL, NULL, NULL);
		if (sServerBusId <= 0)
		{
			llinfos << "Failed to acquire the bus: " << VIEWERAPI_SERVICE
					<< llendl;
			return false;
		}
	}

	return true;
}
