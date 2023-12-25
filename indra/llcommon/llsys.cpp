/**
 * @file llsys.cpp
 * @brief Impelementation of the basic system query functions.
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

#include <sstream>

#if LL_WINDOWS
# if LL_NETBIOS
#  include "llwin32headers.h"
#  include <nb30.h>			// For ADAPTER_STATUS
# else
#  include "llwin32headerslean.h"
# endif
# include <iphlpapi.h>
# include <psapi.h>
# include <VersionHelpers.h>
# include <mmsystem.h>		// For timeBeginPeriod()
# define _interlockedbittestandset _renamed_interlockedbittestandset
# define _interlockedbittestandreset _renamed_interlockedbittestandreset
# include <intrin.h>
# undef _interlockedbittestandset
# undef _interlockedbittestandreset
# include <powerbase.h>
#elif LL_DARWIN
# include <unistd.h>
# include <stdint.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/utsname.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_dl.h>
# include <net/route.h>
# include <ifaddrs.h>
# include <CoreServices/CoreServices.h>
# include <mach/task.h>
# include <mach/mach_host.h>
# include <mach/machine.h>
// Disable warnings about Gestalt calls being deprecated until Apple gets on
// the ball and provides an alternative
# pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif LL_LINUX
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <sched.h>			// For sched_setaffinity() & getcpu() when present
# ifndef getcpu				// ... else we need syscall():
#  include <sys/syscall.h>
# endif
# include <sys/utsname.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
# include <netinet/in.h>
# include <linux/sockios.h>
#endif

#include "boost/thread.hpp"

#include "linden_common.h"

#include "llsys.h"

#include "llmemory.h"
#include "llsd.h"
#include "llthread.h"			// For *_main_thread()
#include "lltimer.h"

///////////////////////////////////////////////////////////////////////////////
// LLOSInfo class
///////////////////////////////////////////////////////////////////////////////

#if LL_WINDOWS

#ifndef DLLVERSIONINFO
typedef struct _DllVersionInfo
{
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;
#endif

#ifndef DLLGETVERSIONPROC
typedef int (FAR WINAPI* DLLGETVERSIONPROC) (DLLVERSIONINFO*);
#endif

// When running under Wine, this function returns a pointer on the Wine version
// number, or NULL otherwise. HB
static const char* get_wine_version()
{
	HMODULE h_dll_inst = GetModuleHandle(TEXT("ntdll.dll"));
	if (!h_dll_inst)
	{
		// Should never happen under Win7+...
		llwarns << "Could not load ntdll.dll; cannot determine if running under Wine."
				<< llendl;
		return NULL;
	}
	typedef const char* (FAR WINAPI* DLLGETWINEVERPROC)();
	DLLGETWINEVERPROC wine_get_versionp =
		(DLLGETWINEVERPROC)GetProcAddress(h_dll_inst, "wine_get_version");
	if (!wine_get_versionp)
	{
		return NULL;
	}
	return (*wine_get_versionp)();
}

#endif // LL_WINDOWS

LLOSInfo::LLOSInfo()
{
#if LL_WINDOWS
	S32 major = 0;
	S32 minor = 0;
	S32 build = 0;
	if (IsWindows10OrGreater())
	{
		major = 10;
		mOSStringSimple = "Windows 10 ";
	}
	else if (IsWindows8Point1OrGreater())
	{
		major = 6;
		minor = 3;
		if (IsWindowsServer())
		{
			mOSStringSimple = "Windows Server 2012 R2 ";
		}
		else
		{
			mOSStringSimple = "Windows 8.1 ";
		}
	}
	else if (IsWindows8OrGreater())
	{
		major = 6;
		minor = 2;
		if (IsWindowsServer())
		{
			mOSStringSimple = "Windows Server 2012 ";
		}
		else
		{
			mOSStringSimple = "Windows 8 ";
		}
	}
	else if (IsWindows7SP1OrGreater())
	{
		major = 6;
		minor = 1;
		if (IsWindowsServer())
		{
			mOSStringSimple = "Windows Server 2008 R2 SP1 ";
		}
		else
		{
			mOSStringSimple = "Windows 7 SP1 ";
		}		
	}
	else if (IsWindows7OrGreater())
	{
		major = 6;
		minor = 1;
		if (IsWindowsServer())
		{
			mOSStringSimple = "Windows Server 2008 R2 ";
		}
		else
		{
			mOSStringSimple = "Windows 7 ";
		}		
	}
	else if (IsWindowsVistaSP2OrGreater())
	{
		major = 6;
		if (IsWindowsServer())
		{
			mOSStringSimple = "Windows Server 2008 SP2 ";
		}
		else
		{
			mOSStringSimple = "Windows Vista SP2 ";
		}		
	}
	else
	{
		mOSStringSimple = "Windows unsupported version ";
	}

	// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	bool success = GetVersionEx((OSVERSIONINFO*)&osvi) != 0;
	if (!success)
	{
		// If OSVERSIONINFOEX does not work, try OSVERSIONINFO.
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		success = GetVersionEx((OSVERSIONINFO*)&osvi) != 0;
	}
	if (success)
	{
		build = osvi.dwBuildNumber & 0xffff;
		if (major == 10 && build >= 22000)
		{
			major = 11;
			mOSStringSimple = "Windows 11 ";
		}
	}
	else
	{
		llwarns << "Could not get Windows build via GetVersionEx()."
				<< llendl;
	}

	mInaccurateSleep = false;
	DWORD revision = 0;
	if (major >= 10)
	{
		// Windows 10 and later got an inaccurate Sleep() call by default...
		// Let's change the minimum resolution to 1ms. HB
		mInaccurateSleep = true;
		if (timeBeginPeriod(1) == TIMERR_NOCANDO)
		{
			llwarns << "Could not set the Sleep() resolution to 1ms."
					<< llendl;
		}

		// For Windows 10+, get the update build revision from the registry
		HKEY key;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
						  TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
						  0, KEY_READ, &key) == ERROR_SUCCESS)
		{
			DWORD cb_data(sizeof(DWORD));
			if (RegQueryValueExW(key, L"UBR", 0, NULL,
								 reinterpret_cast<LPBYTE>(&revision),
								 &cb_data) != ERROR_SUCCESS)
			{
				revision = 0;
			}
		}
	}
	if (revision)
	{
		mOSVersionString = llformat("%d.%d (build %d.%d)", major, minor, build,
									(S32)revision);
	}
	else
	{
		mOSVersionString = llformat("%d.%d (build %d)", major, minor, build);
	}
	mOSStringSimple += "64 bits ";
	mOSString = "Microsoft " + mOSStringSimple + "v" + mOSVersionString;

	// Check for Wine emulation (needed to work around bugs in Wine). HB
	const char* wine_ver = get_wine_version();
	mUnderWine = wine_ver != NULL;
	if (mUnderWine)
	{
		mWineVersionString.assign(wine_ver);
		mOSString += " (Wine v" + mWineVersionString + ")";
	}
#elif LL_DARWIN
	// Initialize mOSStringSimple to something like:
	// "Mac OS X 10.6.7"
	{
		SInt32 major_version, minor_version, bugfix_version;
		OSErr r1 = Gestalt(gestaltSystemVersionMajor, &major_version);
		OSErr r2 = Gestalt(gestaltSystemVersionMinor, &minor_version);
		OSErr r3 = Gestalt(gestaltSystemVersionBugFix, &bugfix_version);

		if (r1 == noErr && r2 == noErr && r3 == noErr)
		{
			mOSVersionString = llformat("%d.%d.%d", major_version,
										minor_version, bugfix_version);

			std::string os_version = "Mac OS X " + mOSVersionString;
			// Put it in the OS string we are compiling
			mOSStringSimple.append(os_version);
		}
		else
		{
			mOSStringSimple.append("Unable to collect OS info");
		}
	}

	// Initialize mOSString to something like:
	// "Mac OS X 10.6.7 Darwin Kernel Version 10.7.0:
	// Sat Jan 29 15:17:16 PST 2011; root:xnu-1504.9.37~1/RELEASE_I386 i386"
	struct utsname un;
	if (uname(&un) != -1)
	{
		mOSString = mOSStringSimple;
		mOSString.append(" ");
		mOSString.append(un.sysname);
		mOSString.append(" ");
		mOSString.append(un.release);
		mOSString.append(" ");
		mOSString.append(un.version);
		mOSString.append(" ");
		mOSString.append(un.machine);
	}
	else
	{
		mOSString = mOSStringSimple;
	}
#else	// LL_LINUX
	mVersionMajor = mVersionMinor = 0;
	struct utsname un;
	if (uname(&un) != -1)
	{
		mOSString.assign(un.sysname);
		mOSString.append("-");
		mOSString.append(un.machine);

		// Note: un.release is the actual Linux version (including any string
		// identifying distribution-specific patched kernels), while un.version
		// is just "#<build number> SMP <build date>", which is irrelevant and
		// we do not care the least about... HB
		mOSVersionString.assign(un.release);

		mOSString += " v" + mOSVersionString;

		mOSStringSimple = mOSString;

		std::string version = mOSVersionString;
		size_t i = version.find('.');
		if (i != std::string::npos)
		{
			mVersionMajor = atoi(version.substr(0, i).c_str());
			version.erase(0, i + 1);
			i = version.find('.');
			if (i != std::string::npos)
			{
				mVersionMinor = atoi(version.substr(0, i).c_str());
			}
		}
	}
	else
	{
		mOSStringSimple = "Unable to collect OS info";
		mOSString = mOSStringSimple;
	}
#endif
}

LLOSInfo::~LLOSInfo()
{
#if LL_WINDOWS
	if (mInaccurateSleep)
	{
		// On process exit, restore default by calling this to match the
		// timeBeginPeriod(1) above. HB
		timeEndPeriod(1);
	}
#endif
}

//static
S32 LLOSInfo::getNodeID(unsigned char* node_id)
{
	static unsigned char sNodeID[8];
	static S32 result = -2;
	if (result == -2)	// First call.
	{
		result = getOSNodeID(sNodeID);
	}
	if (result == 1)
	{
		memcpy(node_id, sNodeID, 6);
	}
	return result;
}

#if LL_LINUX

//static
S32 LLOSInfo::getOSNodeID(unsigned char* node_id)
{
	int n, i;
	unsigned char* a;

// BSD 4.4 defines the size of an ifreq to be the max of sizeof(ifreq)
// and sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len. However, under earlier
// systems, sa_len is not present, so the size is just sizeof(struct ifreq).
#ifdef HAVE_SA_LEN
# define ifreq_size(i) llmax(sizeof(struct ifreq),\
							 sizeof((i).ifr_name) + (i).ifr_addr.sa_len)
#else
# define ifreq_size(i) sizeof(struct ifreq)
#endif // HAVE_SA_LEN

	int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sd < 0)
	{
		return -1;
	}

	char buf[1024];
	memset(buf, 0, sizeof(buf));

	struct ifreq ifr, *ifrp;
	struct ifconf ifc;
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl (sd, SIOCGIFCONF, (char*)&ifc) < 0)
	{
		close(sd);
		return -1;
	}
	n = ifc.ifc_len;
	for (i = 0; i < n; i+= ifreq_size(*ifr))
	{
		ifrp = (struct ifreq*)((char*) ifc.ifc_buf + i);
		strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);
#ifdef SIOCGIFHWADDR
		if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
			continue;
		a = (unsigned char*)&ifr.ifr_hwaddr.sa_data;
#else
# ifdef SIOCGENADDR
		if (ioctl(sd, SIOCGENADDR, &ifr) < 0)
			continue;
		a = (unsigned char*)ifr.ifr_enaddr;
# else	// We do not have a way of getting the hardware address
		close(sd);
		return 0;
# endif	// SIOCGENADDR
#endif	// SIOCGIFHWADDR
		if (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5])
		{
			continue;
		}
		if (node_id)
		{
			memcpy(node_id, a, 6);
			close(sd);
			return 1;
		}
	}
	close(sd);
	return 0;
}

#elif LL_WINDOWS

# if LL_NETBIOS

typedef struct _ASTAT_
{
	ADAPTER_STATUS adapt;
	NAME_BUFFER	 NameBuff [30];
} ASTAT, * PASTAT;

//static
S32 LLOSInfo::getOSNodeID(unsigned char* node_id)
{
	NCB Ncb;
	memset(&Ncb, 0, sizeof(Ncb));
	Ncb.ncb_command = NCBENUM;
	LANA_ENUM lenum;
	Ncb.ncb_buffer = (UCHAR*)&lenum;
	Ncb.ncb_length = sizeof(lenum);
	UCHAR ret_code = Netbios(&Ncb);

	S32 retval = 0;
	ASTAT Adapter;
	for (S32 i = 0; i < (S32)lenum.length; ++i)
	{
		memset(&Ncb, 0, sizeof(Ncb));
		Ncb.ncb_command = NCBRESET;
		Ncb.ncb_lana_num = lenum.lana[i];

		ret_code = Netbios(&Ncb);

		memset(&Ncb, 0, sizeof (Ncb));
		Ncb.ncb_command = NCBASTAT;
		Ncb.ncb_lana_num = lenum.lana[i];

		strcpy((char*)Ncb.ncb_callname, "*				  ");
		Ncb.ncb_buffer = (unsigned char*)&Adapter;
		Ncb.ncb_length = sizeof(Adapter);

		ret_code = Netbios(&Ncb);
		if (ret_code == 0)
		{
			memcpy(node_id, Adapter.adapt.adapter_address, 6);
			retval = 1;
		}
	}
	return retval;
}

# else // LL_NETBIOS

//static
S32 LLOSInfo::getOSNodeID(unsigned char* node_id)
{
	static bool got_node_id = false;
	static unsigned char local_node_id[6];
	if (got_node_id)
	{
		memcpy(node_id, local_node_id, sizeof(local_node_id));
		return 1;
	}

	ULONG out_buf_len = 0U;
	ULONG family = AF_INET;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
	GetAdaptersAddresses(AF_INET, flags, NULL, NULL, &out_buf_len);

	PIP_ADAPTER_ADDRESSES addrp =
		reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(out_buf_len));
	if (!addrp)
	{
		return 0;
	}

	DWORD dw_ret_val = 0;
	for (U32 i = 0; i < 3; ++i)
	{
		dw_ret_val = GetAdaptersAddresses(family, flags, NULL, addrp,
										  &out_buf_len);
		if (dw_ret_val != ERROR_BUFFER_OVERFLOW)
		{
			break;
		}
	}

	S32 retval = 0;
	if (dw_ret_val == NO_ERROR)
	{
		PIP_ADAPTER_ADDRESSES curaddrp = addrp;
		do
		{
			if (curaddrp->FirstGatewayAddress &&
				curaddrp->OperStatus == IfOperStatusUp &&
				curaddrp->PhysicalAddressLength == 6 &&
				(curaddrp->ConnectionType == NET_IF_CONNECTION_DEDICATED &&
				 (curaddrp->IfType == IF_TYPE_ETHERNET_CSMACD ||
				  curaddrp->IfType == IF_TYPE_IEEE80211)))
			{
				for (U32 i = 0; i < 6; ++i)
				{
					node_id[i] = local_node_id[i] =
						curaddrp->PhysicalAddress[i];
				}
				retval = 1;
				got_node_id = true;
				break;
			}
			curaddrp = curaddrp->Next;
		}
		while (curaddrp);
	}

	free(addrp);

	return retval;
}
# endif // LL_NETBIOS

#elif LL_DARWIN
//static
S32 LLOSInfo::getOSNodeID(unsigned char* node_id)
{
	unsigned char* a = NULL;
	struct ifaddrs *ifap, *ifa;
	int rv;
	S32 result = 0;

	if ((rv = getifaddrs(&ifap)) == -1)
	{
		return -1;
	}
	if (ifap == NULL)
	{
		return -1;
	}

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr->sa_family == AF_LINK)
		{
			// This is a link-level address
			struct sockaddr_dl* lla = (struct sockaddr_dl*)ifa->ifa_addr;
			if (lla->sdl_type == IFT_ETHER)
			{
				// Use the first ethernet MAC in the list.
				// For some reason, the macro LLADDR() defined in net/if_dl.h
				// does not expand correctly.  This is what it would do.
				a = (unsigned char*)&((lla)->sdl_data);
				a += (lla)->sdl_nlen;

				if (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5])
				{
					continue;
				}

				if (node_id)
				{
					memcpy(node_id, a, 6);
					result = 1;
				}

				// We found one.
				break;
			}
		}
	}
	freeifaddrs(ifap);

	return result;
}

#endif

///////////////////////////////////////////////////////////////////////////////
// LLProcessorInfo class, which used to be in a separate llprocessor.h/cpp
// module, but is only consumed by LLCPUInfo, so I moved it here, where it
// belongs. *TODO: maybe make it into an LLCPUInfo::Impl sub-class ?  HB
///////////////////////////////////////////////////////////////////////////////

class LLProcessorInfo final
{
	friend class LLCPUInfo;

protected:	// Access limited to LLCPUInfo
	LOG_CLASS(LLProcessorInfo);

	LLProcessorInfo();

	F64 getCPUFrequency() const;

	bool hasSSE2() const;
	bool hasSSE3() const;
	bool hasSSE3S() const;
	bool hasSSE41() const;
	bool hasSSE42() const;
	bool hasSSE4a() const;

	U32 getPhysicalCores() const;
	U32 getVirtualCores() const;
	U32 getMaxChildThreads() const;

	std::string getCPUFamilyName() const;
	std::string getCPUBrandName() const;

	// This is virtual to support a different Linux format.
	virtual std::string getCPUFeatureDescription() const;

protected:
	void setInfo(S32 info_type, const LLSD& value);
    LLSD getInfo(S32 info_type, const LLSD& default_val) const;

	void setConfig(S32 config_type, const LLSD& value);
	LLSD getConfig(S32 config_type, const LLSD& default_val) const;

	void setExtension(const std::string& name);
	bool hasExtension(const std::string& name) const;

	// Used to refresh the frequency of the core we are running onto, after
	// it was left enough time to reach its turbo frequency. Returns true when
	// it detected an increase of the frequency. HB
#if LL_LINUX
	bool refreshAffectedCPUFrequency();
#else
	LL_INLINE bool refreshAffectedCPUFrequency()	{ return false; }
#endif

private:
	void getCPUIDInfo();

	void setInfo(const std::string& name, const LLSD& value);
	LLSD getInfo(const std::string& name, const LLSD& defaultVal) const;

	void setConfig(const std::string& name, const LLSD& value);
	LLSD getConfig(const std::string& name, const LLSD& defaultVal) const;

private:
	LLSD mProcessorInfo;
};

enum cpu_info
{
	eBrandName = 0,
	eFrequency,
	eVendor,
	eStepping,
	eFamily,
	eExtendedFamily,
	eModel,
	eExtendedModel,
	eType,
	eBrandID,
	eFamilyName
};

const char* cpu_info_names[] =
{
	"Processor Name",
	"Frequency",
	"Vendor",
	"Stepping",
	"Family",
	"Extended Family",
	"Model",
	"Extended Model",
	"Type",
	"Brand ID",
	"Family Name"
};

enum cpu_config
{
	eMaxID,
	eMaxExtID,
	eCLFLUSHCacheLineSize,
	eAPICPhysicalID,
	eCacheLineSize,
	eL2Associativity,
	eCacheSizeK,
	eFeatureBits,
	eExtFeatureBits
};

const char* cpu_config_names[] =
{
	"Max Supported CPUID level",
	"Max Supported Ext. CPUID level",
	"CLFLUSH cache line size",
	"APIC Physical ID",
	"Cache Line Size",
	"L2 Associativity",
	"Cache Size",
	"Feature Bits",
	"Ext. Feature Bits"
};

// *NOTE: Mani - this contains the elements we reference directly and
// extensions beyond the first 32. The rest of the names are referenced by bit
// mask returned from cpuid.
enum cpu_features
{
	eSSE2_Ext = 26,
	eSSE3_Features = 32,
	eMONTIOR_MWAIT = 33,
	eCPLDebugStore = 34,
	eThermalMonitor2 = 35,
	eSSE3S_Features = 37,
	eSSE4_1_Features = 38,
	eSSE4_2_Features = 39,
	eSSE4a_Features = 40,
};

const char* cpu_feature_names[] =
{
	"x87 FPU On Chip",
	"Virtual-8086 Mode Enhancement",
	"Debugging Extensions",
	"Page Size Extensions",
	"Time Stamp Counter",
	"RDMSR and WRMSR Support",
	"Physical Address Extensions",
	"Machine Check Exception",
	"CMPXCHG8B Instruction",
	"APIC On Chip",
	"Unknown1",
	"SYSENTER and SYSEXIT",
	"Memory Type Range Registers",
	"PTE Global Bit",
	"Machine Check Architecture",
	"Conditional Move/Compare Instruction",
	"Page Attribute Table",
	"Page Size Extension",
	"Processor Serial Number",
	"CFLUSH Extension",
	"Unknown2",
	"Debug Store",
	"Thermal Monitor and Clock Ctrl",
	"MMX Technology",
	"FXSAVE/FXRSTOR",
	"SSE Extensions",
	"SSE2 Extensions",
	"Self Snoop",
	"Hyper-threading Technology",
	"Thermal Monitor",
	"Unknown4",
	"Pend. Brk. EN.",			// 31 End of FeatureInfo bits

	"SSE3 New Instructions",	// 32
	"MONITOR/MWAIT",
	"CPL Qualified Debug Store",
	"Thermal Monitor 2",

	"",							// No more in use (was "Altivec")

	"SSE3S Instructions",
	"SSE4.1 Instructions",
	"SSE4.2 Instructions",
	"SSE4a Instructions",
};

#if SSE2NEON	// ARM CPUs only
static std::string arm_CPUFamilyName(S32 implementer, S32 cpu_part)
{
	std::string name;

	switch (implementer)
	{
		// Table from util-linux' lscpu utility
		case 0x41:	name = "ARM ";		break;
		case 0x42:	name = "Broadcom ";	break;
		case 0x43:	name = "Cavium ";	break;
		case 0x44:	name = "DEC ";		break;
		case 0x4e:	name = "Nvidia ";	break;
		case 0x50:	name = "APM ";		break;
		case 0x51:	name = "Qualcomm ";	break;
		case 0x53:	name = "Samsung ";	break;
		case 0x56:	name = "Marvell ";	break;
		case 0x69:	name = "Intel ";	break;
		default:	name = "Unknown implementer ";
	}

	// Table from https://en.wikipedia.org/wiki/Comparison_of_ARMv8-A_cores
	// We only care about 64 bits (ARM v8+) processors.
	switch (cpu_part)
	{
		case 0xd01:	name += "Cortex-A32";	break;
		case 0xd02:	name += "Cortex-A34";	break;
		case 0xd03:	name += "Cortex-A53";	break;
		case 0xd04:	name += "Cortex-A35";	break;
		case 0xd05:	name += "Cortex-A55";	break;
		case 0xd06:	name += "Cortex-A65";	break;
		case 0xd07:	name += "Cortex-A57";	break;
		case 0xd08:	name += "Cortex-A72";	break;
		case 0xd09:	name += "Cortex-A73";	break;
		case 0xd0a:	name += "Cortex-A75";	break;
		case 0xd0b:	name += "Cortex-A76";	break;
		case 0xd0d:	name += "Cortex-A77";	break;
		case 0xd41:	name += "Cortex-A78";	break;
		case 0xd44:	name += "Cortex-X1";	break;
		default:	name += "unknown model";
	}

	return name;
}
#else
static std::string intel_CPUFamilyName(S32 cpu_part)
{
	switch (cpu_part)
	{
# if 0	// We do not support these (32 bits) CPUs
		case 0x03: return "Intel i386";
		case 0x04: return "Intel i486";
		case 0x05: return "Intel Pentium";
# endif
		case 0x06: return "Intel Pentium Pro/Pentium 2/Pentium 3/Core";
		case 0x07: return "Intel Itanium (IA-64)";
		case 0x0F: return "Intel Pentium 4/Pentium D/Nocona";
		case 0x10: return "Intel Itanium 2 (IA-64)";
	}
	return llformat("Unknown Intel 0x%.2x family", cpu_part);
}

static std::string amd_CPUFamilyName(S32 cpu_part)
{
	switch (cpu_part)
	{
# if 0	// We do not support these (32 bits) CPUs
		case 0x04: return "AMD 80486/5x86";
		case 0x05: return "AMD K5/K6";
		case 0x06: return "AMD K7";
# endif
		case 0x0F: return "AMD K8/Hammer";
		case 0x10: return "AMD K10";
		case 0x11: return "AMD K8/K10 hybrid";
		case 0x12: return "AMD K10 Llano";
		case 0x14: return "AMD Bobcat";
		case 0x15: return "AMD Bulldozer/Piledriver/Steamroller/Excavator";
		case 0x16: return "AMD Jaguar";
		case 0x17: return "AMD Zen/Zen+/Zen2";
		case 0x18: return "AMD Hygon Dhyana";
		case 0x19: return "AMD Zen3/Zen3+/Zen4";
	}
	return llformat("Unknown AMD 0x%.2x family", cpu_part);
}
#endif

#if LL_LINUX
static const char CPUINFO_FILE[] = "/proc/cpuinfo";
static const char CPUFREQ_FILE[] =
	"/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq";

static std::string compute_CPUFamilyName(const char* id,
										 S32 cpu_part)
{
# if SSE2NEON
	std::stringstream s;
	s << std::hex << id;
	S32 cpu_id;
	s >> cpu_id;
	return arm_CPUFamilyName(cpu_id, cpu_part);
# else
	const char* intel_string = "GenuineIntel";
	const char* amd_string = "AuthenticAMD";
	if (!strncmp(id, intel_string, strlen(intel_string)))
	{
		return intel_CPUFamilyName(cpu_part);
	}
	if (!strncmp(id, amd_string, strlen(amd_string)))
	{
		return amd_CPUFamilyName(cpu_part);
	}
# endif
	return llformat("Unknown CPU vendor: %s", id);
}
#else
static std::string compute_CPUFamilyName(const char* cpu_vendor, S32 family,
										 S32 ext_family)
{
	const char* intel_string = "GenuineIntel";
	const char* amd_string = "AuthenticAMD";
	if (!strncmp(cpu_vendor, intel_string, strlen(intel_string)))
	{
		U32 cpu_part = family + ext_family;
		return intel_CPUFamilyName(cpu_part);
	}
	if (!strncmp(cpu_vendor, amd_string, strlen(amd_string)))
	{
		U32 cpu_part = family == 0xF ? family + ext_family : family;
		return amd_CPUFamilyName(cpu_part);
	}
	return llformat("Unknown CPU vendor: %s", cpu_vendor);
}
#endif

#if LL_WINDOWS
typedef struct _PROCESSOR_POWER_INFORMATION
{
   ULONG  Number;
   ULONG  MaxMhz;
   ULONG  CurrentMhz;
   ULONG  MhzLimit;
   ULONG  MaxIdleState;
   ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

// Normally, the return type should be NTSTATUS, but the latter is defined in
// a header that conficts with the already included windows.h header (Windoze
// sucks !)... Since it is just an enum, simply use an int for our typedef. HB
typedef int (FAR WINAPI* CALLNTPOWERINFORMATION)(POWER_INFORMATION_LEVEL,
												 PVOID, ULONG, PVOID, ULONG);

// Returns the detected CPU core frequency in MHz for the core running the
// viewer, or 0.0 in case of failure. 'method' is set to 0 in case of failure,
// 1 when the detection was done via the system power service, or 2 when the
// frequency was measured based on a TSC timing loop. Note: this function does
// not work properly with modern CPUs and their turbo modes. Windows does not
// expose any user-level API to determine their actual, running frequency, and
// determining it by ourselves would mean running the viewer with admin
// priviledges or using a custom (and signed) driver, in order to access the
// MSRs; it would also mean implementing CPU model-specific MSR decoding... HB
static F64 calculate_cpu_frequency(U32 cores, U32& method)
{
	method = 0;
	F64 frequency = 0.0;

	// We set the process and thread priority to the highest available level
	// (realtime priority). Also we set the process affinity to the first
	// processor in the multiprocessor system.
	HANDLE processh = GetCurrentProcess();
	HANDLE threadh = GetCurrentThread();
	unsigned long prio_class = GetPriorityClass(processh);
	int thread_prio = GetThreadPriority(threadh);
	DWORD_PTR process_mask, system_mask;
	GetProcessAffinityMask(processh, &process_mask, &system_mask);

	SetPriorityClass(processh, REALTIME_PRIORITY_CLASS);
	SetThreadPriority(threadh, THREAD_PRIORITY_TIME_CRITICAL);
	DWORD_PTR new_mask = 1;
	SetProcessAffinityMask(processh, new_mask);

	if (cores)
	{
		// We first try to get the actual CPU frequency for the CPU core we are
		// running on via the system power service. Amusingly, this code always
		// works beautifully when running via Wine under Linux, but fails to
		// report the *actual* frequency in CurrentMhz under Windows with modern
		// CPUs (anything after the Core2 Quad, for Intel)... HB
		HINSTANCE h_dll = LoadLibraryW(L"powrprof.dll");
		if (h_dll)
		{
			CALLNTPOWERINFORMATION proc =
				(CALLNTPOWERINFORMATION)GetProcAddress(h_dll,
													   "CallNtPowerInformation");
			if (proc)
			{
				int size = cores * sizeof(PROCESSOR_POWER_INFORMATION);
				LPBYTE bufferp = new BYTE[size];
				if ((*proc)(ProcessorInformation, NULL, 0, bufferp, size) == 0)
				{
					PPROCESSOR_POWER_INFORMATION ppi =
						(PPROCESSOR_POWER_INFORMATION)bufferp;
					frequency = F64(ppi->CurrentMhz);
					method = 1;
				}
				delete[] bufferp;
			}
			FreeLibrary(h_dll);
		}
	}

	// If the above failed, fall back to an unreliable method, involving
	// performance counters (based on the TSC), which is sadly (and just as
	// well as the system power service method) incapable of reporting turbo
	// frequencies on modern CPUs... HB
	if (frequency == 0.0)
	{
		// Check the frequency of the high resolution timer for the measure
		// process.
		LARGE_INTEGER wait;
		if (QueryPerformanceFrequency((LARGE_INTEGER*)&wait))
		{
			wait.QuadPart >>= 7;	// 128ms for a 1ms resolution

			// Call a CPUID to ensure, that all other prior called functions
			// are completed now (serialization).
			int cpu_info[4] = { -1 };
			__cpuid(cpu_info, 0);

			LARGE_INTEGER start, current;
			QueryPerformanceCounter(&start);
			unsigned __int64 start_time = __rdtsc();
			do
			{
				QueryPerformanceCounter(&current);
			}
			while (current.QuadPart - start.QuadPart < wait.QuadPart);
			frequency = F64(((__rdtsc() - start_time) << 7) / 1000000UL);
			method = 2;
		}
	}

	// Now we can restore the default process and thread priorities
	SetProcessAffinityMask(processh, process_mask);
	SetThreadPriority(threadh, thread_prio);
	SetPriorityClass(processh, prio_class);

	return frequency;
}
#elif LL_DARWIN
static int getSysctlInt(const char* name)
{
	int result = 0;
	size_t len = sizeof(int);
	int error = sysctlbyname(name, (void*)&result, &len, NULL, 0);
	return error == -1 ? 0 : result;
}

static uint64_t getSysctlInt64(const char* name)
{
	uint64_t value = 0;
	size_t size = sizeof(value);
	int result = sysctlbyname(name, (void*)&value, &size, NULL, 0);
	if (result == 0)
	{
		if (size == sizeof(uint64_t))
		{
			// Nothing to do
		}
		else if (size == sizeof(uint32_t))
		{
			value = (uint64_t)((uint32_t*)&value);
		}
		else if (size == sizeof(uint16_t))
		{
			value = (uint64_t)((uint16_t*)&value);
		}
		else if (size == sizeof(uint8_t))
		{
			value = (uint64_t)((uint8_t*)&value);
		}
		else
		{
			llwarns << "Unknown type returned from sysctl !" << llendl;
		}
	}

	return result == -1 ? 0 : value;
}
#endif

LLProcessorInfo::LLProcessorInfo()
{
	mProcessorInfo["info"] = LLSD::emptyMap();
	mProcessorInfo["config"] = LLSD::emptyMap();
	mProcessorInfo["extension"] = LLSD::emptyMap();

	getCPUIDInfo();

	S32 threads = boost::thread::hardware_concurrency();
#if LL_WINDOWS
	U32 vcores = threads;
#endif
	mProcessorInfo["virtual_cores"] = threads;
	S32 cores = boost::thread::physical_concurrency();
	mProcessorInfo["physical_cores"] = cores;
	if (!threads)
	{
		llwarns << "Could not determine hardware thread concurrency on this platform !"
				<<  llendl;
		threads = 4;	// Use a sane default: 4 threads
	}
	else if (threads != cores && threads > 4)
	{
		// For multi-core CPUs with SMT and and more than 4 threads, reserve
		// two threads to the main loop.
		threads -= 2;
	}
	else if (threads > 1)
	{
		// For multi-core CPUs without SMT or with 4 threads or less, reserve
		// one core or thread to the main loop.
		--threads;
	}
	mProcessorInfo["max_child_threads"] = threads;

#if LL_WINDOWS
	U32 method = 0;
	setInfo(eFrequency, calculate_cpu_frequency(vcores, method));
	if (method)
	{
		llinfos << "Got the CPU frequency via "
				<< (method == 1 ? "the system power service"
								: "a TSC timing loop")
				<< " (this sadly does not account for turbo modes of modern CPUs)."
				<< llendl;
	}
	else
	{
		llwarns << "Failed to determine the CPU frequency." << llendl;
	}

#elif LL_DARWIN
	U64 frequency = getSysctlInt64("hw.cpufrequency");
	setInfo(eFrequency, (F64)frequency  / (F64)1000000);
#endif
}

F64 LLProcessorInfo::getCPUFrequency() const
{
	return getInfo(eFrequency, 0).asReal();
}

bool LLProcessorInfo::hasSSE2() const
{
	return hasExtension(cpu_feature_names[eSSE2_Ext]);
}

bool LLProcessorInfo::hasSSE3() const
{
	return hasExtension(cpu_feature_names[eSSE3_Features]);
}

bool LLProcessorInfo::hasSSE3S() const
{
	return hasExtension(cpu_feature_names[eSSE3S_Features]);
}

bool LLProcessorInfo::hasSSE41() const
{
	return hasExtension(cpu_feature_names[eSSE4_1_Features]);
}

bool LLProcessorInfo::hasSSE42() const
{
	return hasExtension(cpu_feature_names[eSSE4_2_Features]);
}

bool LLProcessorInfo::hasSSE4a() const
{
	return hasExtension(cpu_feature_names[eSSE4a_Features]);
}

U32 LLProcessorInfo::getPhysicalCores() const
{
	return U32(mProcessorInfo["physical_cores"].asInteger());
}

U32 LLProcessorInfo::getVirtualCores() const
{
	return U32(mProcessorInfo["virtual_cores"].asInteger());
}

U32 LLProcessorInfo::getMaxChildThreads() const
{
	return U32(mProcessorInfo["max_child_threads"].asInteger());
}

std::string LLProcessorInfo::getCPUFamilyName() const
{
	return getInfo(eFamilyName, "Unset family").asString();
}

std::string LLProcessorInfo::getCPUBrandName() const
{
	return getInfo(eBrandName, "Unset brand").asString();
}

void LLProcessorInfo::setInfo(S32 info_type, const LLSD& value)
{
	setInfo(cpu_info_names[info_type], value);
}

LLSD LLProcessorInfo::getInfo(S32 info_type, const LLSD& defaultVal) const
{
	return getInfo(cpu_info_names[info_type], defaultVal);
}

void LLProcessorInfo::setConfig(S32 config_type, const LLSD& value)
{
	setConfig(cpu_config_names[config_type], value);
}

LLSD LLProcessorInfo::getConfig(S32 config_type, const LLSD& defaultVal) const
{
	return getConfig(cpu_config_names[config_type], defaultVal);
}

void LLProcessorInfo::setExtension(const std::string& name)
{
	mProcessorInfo["extension"][name] = "true";
}

bool LLProcessorInfo::hasExtension(const std::string& name) const
{
	return mProcessorInfo["extension"].has(name);
}

void LLProcessorInfo::setInfo(const std::string& name, const LLSD& value)
{
	mProcessorInfo["info"][name] = value;
}

LLSD LLProcessorInfo::getInfo(const std::string& name,
							  const LLSD& default_val) const
{
	if (mProcessorInfo["info"].has(name))
	{
		return mProcessorInfo["info"][name];
	}
	return default_val;
}

void LLProcessorInfo::setConfig(const std::string& name, const LLSD& value)
{
	mProcessorInfo["config"][name] = value;
}

LLSD LLProcessorInfo::getConfig(const std::string& name,
								const LLSD& default_val) const
{
	LLSD r = mProcessorInfo["config"].get(name);
	return r.isDefined() ? r : default_val;
}

#if LL_LINUX
std::string LLProcessorInfo::getCPUFeatureDescription() const
{
	std::ostringstream s;

	// *NOTE:Mani - This is for linux only.
	LLFILE* cpuinfo = LLFile::open(CPUINFO_FILE, "rb");
	if (cpuinfo)
	{
		constexpr size_t BUFFER_LEN = 32768;
		char line[BUFFER_LEN];
		memset(line, 0, BUFFER_LEN);
		while(fgets(line, BUFFER_LEN, cpuinfo))
		{
			line[strlen(line) - 1] = ' ';
			s << line;
			s << std::endl;
		}
		LLFile::close(cpuinfo);
		s << std::endl;
	}
	else
	{
		s << "Unable to collect processor information" << std::endl;
	}
	return s.str();
}
#else
std::string LLProcessorInfo::getCPUFeatureDescription() const
{
	std::ostringstream out;
	out << std::endl << std::endl;
	out << "// CPU General Information" << std::endl;
	out << "//////////////////////////" << std::endl;
	out << "Processor Name:   " << getCPUBrandName() << std::endl;
	out << "Frequency:        " << getCPUFrequency() << " MHz"
		<< std::endl;
	out << "Vendor:			  "
		<< getInfo(eVendor, "Unset vendor").asString()
		<< std::endl;
	out << "Family:           " << getCPUFamilyName() << " ("
		<< getInfo(eFamily, 0) << ")" << std::endl;
	out << "Extended family:  " << getInfo(eExtendedFamily, 0).asInteger()
		<< std::endl;
	out << "Model:            " << getInfo(eModel, 0).asInteger() << std::endl;
	out << "Extended model:   " << getInfo(eExtendedModel, 0).asInteger()
								<< std::endl;
	out << "Type:             " << getInfo(eType, 0).asInteger() << std::endl;
	out << "Brand ID:         " << getInfo(eBrandID, 0).asInteger()
								<< std::endl;
	out << std::endl;
	out << "// CPU Configuration" << std::endl;
	out << "//////////////////////////" << std::endl;

	// Iterate through the dictionary of configuration options.
	LLSD configs = mProcessorInfo["config"];
	for (LLSD::map_const_iterator cfgItr = configs.beginMap();
		 cfgItr != configs.endMap(); ++cfgItr)
	{
		out << cfgItr->first << " = " << cfgItr->second.asInteger()
			<< std::endl;
	}
	out << std::endl;

	out << "// CPU Extensions" << std::endl;
	out << "//////////////////////////" << std::endl;

	for (LLSD::map_const_iterator
			itr = mProcessorInfo["extension"].beginMap();
		 itr != mProcessorInfo["extension"].endMap(); ++itr)
	{
		out << "  " << itr->first << std::endl;
	}
	return out.str();
}
#endif	// LL_LINUX

#if LL_LINUX
static std::map<std::string, std::string> sCPUInfo;

static S32 get_affected_cpu_info()
{
	// By default (-1), no info on the CPU/core we are running onto.
	S32 result = -1;
	unsigned int cpu = 0;
# if defined(getcpu)			// glibc v2.29+
	if (getcpu(&cpu, NULL) == 0)
	{
		result = (S32)cpu;
	}
# elif defined(SYS_getcpu)		// Linux after v2.6.20
	if (syscall(SYS_getcpu, &cpu, NULL, NULL) != -1)
	{
		result = (S32)cpu;
	}
# endif
	std::string next_cpu = llformat("%d", cpu + 1);

	LLFILE* cpuinfo_fp = LLFile::open(CPUINFO_FILE, "rb");
	if (!cpuinfo_fp)
	{
		return -2;	// Code for no CPUINFO_FILE file !
	}

	sCPUInfo.clear();

	bool has_freq = false;
	constexpr size_t BUFFER_LEN = 32768;
	char line[BUFFER_LEN];
	memset(line, 0, BUFFER_LEN);
	while (fgets(line, BUFFER_LEN, cpuinfo_fp))
	{
		// /proc/cpuinfo on Linux looks like:
		// name\t*: value\n
		char* tabspot = strchr(line, '\t');
		if (!tabspot) continue;

		char* colspot = strchr(tabspot, ':');
		if (!colspot) continue;

		char* spacespot = strchr(colspot, ' ');
		if (!spacespot) continue;

		char* nlspot = strchr(line, '\n');
		if (!nlspot)
		{
			// Fallback to terminating NUL
			nlspot = line + strlen(line);
		}
		std::string linename(line, tabspot);
		std::string llinename(linename);
		LLStringUtil::toLower(llinename);
		std::string lineval(spacespot + 1, nlspot);
		if (llinename == "processor" && lineval == next_cpu)
		{
			break;
		}
		sCPUInfo[llinename] = lineval;
		if (!has_freq && llinename == "cpu mhz")
		{
			has_freq = true;
		}
	}
	LLFile::close(cpuinfo_fp);

	if (!has_freq)
	{
		// Try cpufreq instead...
		llifstream s(llformat(CPUFREQ_FILE, cpu).c_str());
		if (s.is_open())
		{
			S32 freq;
			s >> freq;
			sCPUInfo["cpu mhz"] = llformat("%.2f", F32(freq) * 0.001f);
		}
	}

	return result;
}

void LLProcessorInfo::getCPUIDInfo()
{
	S32 cpu = get_affected_cpu_info();
	if (cpu == -2)
	{
		llwarns << "Could not get any CPU information: " << CPUINFO_FILE
				<< " file not found !" << llendl;
		return;
	}

	if (cpu >= 0)
	{
		llinfos << "Running on CPU/core #" << cpu << llendl;
	}
	else
	{
		llwarns << "Could not determine on which CPU/core we are running."
				<< llendl;
		cpu = 0;	// Use the info for the first core...
	}

	F64 mhz = 0.0;
	if (LLStringUtil::convertToF64(sCPUInfo["cpu mhz"], mhz) &&
		mhz > 200.0 && mhz < 10000.0)
	{
	    setInfo(eFrequency, mhz);
	}

#define LLPI_SET_INFO_STRING(llpi_id, cpuinfo_id) \
	if (!sCPUInfo[cpuinfo_id].empty()) \
	{ setInfo(llpi_id, sCPUInfo[cpuinfo_id]);}

#define LLPI_SET_INFO_INT(llpi_id, cpuinfo_id) \
	{\
		S32 result; \
		if (!sCPUInfo[cpuinfo_id].empty() \
			&& LLStringUtil::convertToS32(sCPUInfo[cpuinfo_id], result)) \
	    { setInfo(llpi_id, result);} \
	}

	LLPI_SET_INFO_STRING(eBrandName, "model name");
	LLPI_SET_INFO_STRING(eVendor, "vendor_id");

	LLPI_SET_INFO_INT(eStepping, "stepping");
	LLPI_SET_INFO_INT(eModel, "model");

	S32 family = 0;
# if SSE2NEON
	// No "vendor_id" in /proc/cpuinfo for ARM: use "cpu implementer"
	// instead. HB
	const std::string& id = sCPUInfo["cpu implementer"];
	// No "cpu family" in /proc/cpuinfo for ARM: use "cpu part" instead to
	// identify the CPU model. HB
	if (!sCPUInfo["cpu part"].empty())
	{
		std::stringstream s;
		s << std::hex << sCPUInfo["cpu part"];
		s >> family;
		setInfo(eFamily, family);
	}
#else
	const std::string& id = sCPUInfo["vendor_id"];
	if (!sCPUInfo["cpu family"].empty() &&
		LLStringUtil::convertToS32(sCPUInfo["cpu family"], family))
	{
		setInfo(eFamily, family);
	}
#endif
	std::string cpu_family = compute_CPUFamilyName(id.c_str(), family);
	setInfo(eFamilyName, cpu_family);
#if SSE2NEON
	// There is no "model name" in /proc/cpuinfo for ARM: use the family name
	// we just got, instead... HB
	setInfo(eBrandName, cpu_family);
#endif

	// Read extensions
	std::string flags = " " + sCPUInfo["flags"] + " ";
	LLStringUtil::toLower(flags);
# if SSE2NEON	// Force SSE2 flag on if we got Neon-translated SSE2... HB
	setExtension(cpu_feature_names[eSSE2_Ext]);
# else
	if (flags.find(" sse2 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE2_Ext]);
	}
	if (flags.find(" pni ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE3_Features]);
	}
	if (flags.find(" ssse3 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE3S_Features]);
	}
	if (flags.find(" sse4_1 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4_1_Features]);
	}
	if (flags.find(" sse4_2 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4_2_Features]);
	}
	if (flags.find(" sse4a ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4a_Features]);
	}
# endif
}

bool LLProcessorInfo::refreshAffectedCPUFrequency()
{
	S32 cpu = get_affected_cpu_info();
	if (cpu >= 0)
	{
		// Do not account for this little a change in frequency (in MHz). HB
		constexpr F64 CPU_FREQ_JITTER = 16.0;
		F64 mhz;
		if (LLStringUtil::convertToF64(sCPUInfo["cpu mhz"], mhz) &&
			mhz > 200.0 && mhz < 10000.0 &&
			mhz > getCPUFrequency() + CPU_FREQ_JITTER)
		{
		    setInfo(eFrequency, mhz);
			llinfos << "Detected increased CPU/core frequency: " << mhz
					<< "MHz" << llendl;
			return true;
		}
	}
	return false;
}

#elif LL_DARWIN
void LLProcessorInfo::getCPUIDInfo()
{
	size_t len = 0;

	char cpu_brand_string[0x40];
	len = sizeof(cpu_brand_string);
	memset(cpu_brand_string, 0, len);
	sysctlbyname("machdep.cpu.brand_string",
				 (void*)cpu_brand_string, &len, NULL, 0);
	cpu_brand_string[0x3f] = 0;
	setInfo(eBrandName, cpu_brand_string);

	char cpu_vendor[0x20];
	len = sizeof(cpu_vendor);
	memset(cpu_vendor, 0, len);
	sysctlbyname("machdep.cpu.vendor", (void*)cpu_vendor, &len, NULL, 0);
	cpu_vendor[0x1f] = 0;
	setInfo(eVendor, cpu_vendor);

	setInfo(eStepping, getSysctlInt("machdep.cpu.stepping"));
	setInfo(eModel, getSysctlInt("machdep.cpu.model"));
	int family = getSysctlInt("machdep.cpu.family");
	int ext_family = getSysctlInt("machdep.cpu.extfamily");
	setInfo(eFamily, family);
	setInfo(eExtendedFamily, ext_family);
	setInfo(eFamilyName,
			compute_CPUFamilyName(cpu_vendor, family, ext_family));
	setInfo(eExtendedModel, getSysctlInt("machdep.cpu.extmodel"));
	setInfo(eBrandID, getSysctlInt("machdep.cpu.brand"));
	setInfo(eType, 0); // ? where to find this?

	setConfig(eCacheLineSize, getSysctlInt("machdep.cpu.cache.linesize"));
	setConfig(eL2Associativity,
			  getSysctlInt("machdep.cpu.cache.L2_associativity"));
	setConfig(eCacheSizeK, getSysctlInt("machdep.cpu.cache.size"));

	uint64_t feature_info = getSysctlInt64("machdep.cpu.feature_bits");
	S32* feature_infos = (S32*)(&feature_info);
	setConfig(eFeatureBits, feature_infos[0]);

	char cpu_features[1024];
	len = sizeof(cpu_features);
	memset((void*)cpu_features, 0, len);
	sysctlbyname("machdep.cpu.features", (void*)cpu_features, &len, NULL, 0);
	std::string cpu_features_str(cpu_features);
	cpu_features_str = " " + cpu_features_str + " ";
	if (cpu_features_str.find(" SSE3 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE3_Features]);
	}
	if (cpu_features_str.find(" SSE3 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE3_Features]);
	}
	if (cpu_features_str.find(" SSSE3 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE3S_Features]);
	}
	if (cpu_features_str.find(" SSE4.1 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4_1_Features]);
	}
	if (cpu_features_str.find(" SSE4.2 ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4_2_Features]);
	}
	if (cpu_features_str.find(" SSE4A ") != std::string::npos)
	{
		setExtension(cpu_feature_names[eSSE4a_Features]);
	}

	for (unsigned int index = 0, bit = 1; index < eSSE3_Features;
		 ++index, bit <<= 1)
	{
		if (feature_info & bit)
		{
			setExtension(cpu_feature_names[index]);
		}
	}

	uint64_t ext_feature_info = getSysctlInt64("machdep.cpu.extfeature_bits");
	S32 *ext_feature_infos = (S32*)(&ext_feature_info);
	setConfig(eExtFeatureBits, ext_feature_infos[0]);
}
#elif LL_WINDOWS
void LLProcessorInfo::getCPUIDInfo()
{
	// http://msdn.microsoft.com/en-us/library/hskdteyh(VS.80).aspx

	// __cpuid with an InfoType argument of 0 returns the number of
	// valid Ids in cpu_info[0] and the CPU identification string in
	// the other three array elements. The CPU identification string is
	// not in linear order. The code below arranges the information
	// in a human readable form.
	int cpu_info[4] = { -1 };
	__cpuid(cpu_info, 0);
	unsigned int ids = (unsigned int)cpu_info[0];
	setConfig(eMaxID, (S32)ids);

	char cpu_vendor[0x20];
	memset(cpu_vendor, 0, sizeof(cpu_vendor));
	*((int*)cpu_vendor) = cpu_info[1];
	*((int*)(cpu_vendor + 4)) = cpu_info[3];
	*((int*)(cpu_vendor + 8)) = cpu_info[2];
	setInfo(eVendor, cpu_vendor);
	bool is_amd = !strncmp(cpu_vendor, "AuthenticAMD", 12);

	// Get the information associated with each valid Id
	for (unsigned int i = 0; i <= ids; ++i)
	{
		__cpuid(cpu_info, i);

		// Interpret CPU feature information.
		if  (i == 1)
		{
			setInfo(eStepping, cpu_info[0] & 0xf);
			setInfo(eModel, (cpu_info[0] >> 4) & 0xf);
			int family = (cpu_info[0] >> 8) & 0xf;
			setInfo(eFamily, family);
			setInfo(eType, (cpu_info[0] >> 12) & 0x3);
			setInfo(eExtendedModel, (cpu_info[0] >> 16) & 0xf);
			int ext_family = (cpu_info[0] >> 20) & 0xff;
			setInfo(eExtendedFamily, ext_family);
			setInfo(eBrandID, cpu_info[1] & 0xff);

			setInfo(eFamilyName,
					compute_CPUFamilyName(cpu_vendor, family, ext_family));

			setConfig(eCLFLUSHCacheLineSize,
					  ((cpu_info[1] >> 8) & 0xff) * 8);
			setConfig(eAPICPhysicalID, (cpu_info[1] >> 24) & 0xff);

			if (cpu_info[2] & 0x1)
			{
				setExtension(cpu_feature_names[eSSE3_Features]);
			}

			if (cpu_info[2] & 0x8)
			{
				// Intel specific SSE3 suplements
				setExtension(cpu_feature_names[eMONTIOR_MWAIT]);
			}

			if (cpu_info[2] & 0x10)
			{
				setExtension(cpu_feature_names[eCPLDebugStore]);
			}

			if (cpu_info[2] & 0x100)
			{
				setExtension(cpu_feature_names[eThermalMonitor2]);
			}

			if (cpu_info[2] & 0x200)
			{
				setExtension(cpu_feature_names[eSSE3S_Features]);
			}

			if (cpu_info[2] & 0x80000)
			{
				setExtension(cpu_feature_names[eSSE4_1_Features]);
			}

			if (cpu_info[2] & 0x100000)
			{
				setExtension(cpu_feature_names[eSSE4_2_Features]);
			}

			unsigned int feature_info = (unsigned int) cpu_info[3];
			for (unsigned int index = 0, bit = 1; index < eSSE3_Features;
				 ++index, bit <<= 1)
			{
				if (feature_info & bit)
				{
					setExtension(cpu_feature_names[index]);
				}
			}
		}
	}

	// Calling __cpuid with 0x80000000 as the InfoType argument gets the number
	// of valid extended IDs.
	__cpuid(cpu_info, 0x80000000);
	unsigned int ext_ids = cpu_info[0];
	setConfig(eMaxExtID, 0);

	char cpu_brand_string[0x40];
	memset(cpu_brand_string, 0, sizeof(cpu_brand_string));

	// Get the information associated with each extended ID.
	for (unsigned int i = 0x80000000; i <= ext_ids; ++i)
	{
		__cpuid(cpu_info, i);

		// Interpret CPU brand string and cache information.
		if (i == 0x80000001)
		{
			if (is_amd)
			{
				setExtension(cpu_feature_names[eSSE4a_Features]);
			}
		}
		else if (i == 0x80000002)
		{
			memcpy(cpu_brand_string, cpu_info, sizeof(cpu_info));
		}
		else if (i == 0x80000003)
		{
			memcpy(cpu_brand_string + 16, cpu_info, sizeof(cpu_info));
		}
		else if (i == 0x80000004)
		{
			memcpy(cpu_brand_string + 32, cpu_info, sizeof(cpu_info));
			setInfo(eBrandName, cpu_brand_string);
		}
		else if (i == 0x80000006)
		{
			setConfig(eCacheLineSize, cpu_info[2] & 0xff);
			setConfig(eL2Associativity, (cpu_info[2] >> 12) & 0xf);
			setConfig(eCacheSizeK, (cpu_info[2] >> 16) & 0xffff);
		}
	}
}
#endif // LL_WINDOWS

///////////////////////////////////////////////////////////////////////////////
// LLCPUInfo class
///////////////////////////////////////////////////////////////////////////////

// Static members
U32 LLCPUInfo::sMainThreadAffinityMask = 0;
bool LLCPUInfo::sMainThreadAffinitySet = false;

LLCPUInfo::LLCPUInfo()
:	mImpl(new LLProcessorInfo)
{
	mHasSSE2 = mImpl->hasSSE2();
	mHasSSE3 = mImpl->hasSSE3();
	mHasSSE3S = mImpl->hasSSE3S();
	mHasSSE41 = mImpl->hasSSE41();
	mHasSSE42 = mImpl->hasSSE42();
	mHasSSE4a = mImpl->hasSSE4a();
	mCPUMHz = mImpl->getCPUFrequency();
	mFamily = mImpl->getCPUFamilyName();
	mCPUString = mImpl->getCPUBrandName();
	mPhysicalCores = mImpl->getPhysicalCores();
	mVirtualCores = mImpl->getVirtualCores();
	mMaxChildThreads = mImpl->getMaxChildThreads();
}

LLCPUInfo::~LLCPUInfo()
{
	delete mImpl;
	mImpl = NULL;
}

S32 LLCPUInfo::affectedCoreFreqDelta()
{
	if (mImpl->refreshAffectedCPUFrequency())
	{
		F64 old_freq = mCPUMHz;
		mCPUMHz = mImpl->getCPUFrequency();
		return (S32)(mCPUMHz - old_freq);
	}
	return 0;
}

std::string LLCPUInfo::getCPUString(bool update_freq)
{
	// If requested (i.e. we are not in a hurry), check (and possibly update)
	// the affected CPU/core frequency stat for when where we want a more
	// accurate/current figure (e.g. in the About floater). HB
	if (update_freq)
	{
		affectedCoreFreqDelta();
	}
	// *NOTE: cpu speed is often way wrong, do a sanity check
	if (mCPUMHz <= 200 || mCPUMHz >= 10000)
	{
		return mCPUString;
	}
	return mCPUString + llformat(" (%d MHz)", (S32)mCPUMHz);
}

LLSD LLCPUInfo::getSSEVersions() const
{
	LLSD sse_versions;
#if SSE2NEON
	// Let's report Neon for AMR64 builds... HB
	sse_versions.append("Neon");
#else
	if (mHasSSE2)
	{
		// All x86_64 CPUs got SSE1... and SSE2...
		// *TODO: eliminate the SSE2 test ?  HB
		sse_versions.append("1");
		sse_versions.append("2");
	}
	if (mHasSSE3)
	{
		sse_versions.append("3");
	}
	if (mHasSSE3S)
	{
		sse_versions.append("3S");
	}
	if (mHasSSE41)
	{
		sse_versions.append("4.1");
	}
	if (mHasSSE42)
	{
		sse_versions.append("4.2");
	}
	if (mHasSSE4a)
	{
		sse_versions.append("4a");
	}
#endif
	return sse_versions;
}

std::string LLCPUInfo::getInfo() const
{
	std::ostringstream s;
	if (mImpl)
	{
		// Gather machine information.
		s << mImpl->getCPUFeatureDescription();

		// These are interesting as they reflect our internal view of the
		// CPU's attributes regardless of platform
		s << "->mHasSSE2:   " << (U32)mHasSSE2 << std::endl;
		s << "->mHasSSE3:   " << (U32)mHasSSE3 << std::endl;
		s << "->mHasSSE3S:  " << (U32)mHasSSE3S << std::endl;
		s << "->mHasSSE41:  " << (U32)mHasSSE41 << std::endl;
		s << "->mHasSSE42:  " << (U32)mHasSSE42 << std::endl;
		s << "->mHasSSE4a:  " << (U32)mHasSSE4a << std::endl;
		s << "->mCPUMHz:    " << mCPUMHz << std::endl;
		s << "->mCPUString: " << mCPUString << std::endl;
	}
	return s.str();
}

//static
void LLCPUInfo::setMainThreadCPUAffinifty(U32 cpu_mask)
{
	assert_main_thread(); // Must be called from the main thread only !

#if LL_LINUX || LL_WINDOWS
	sMainThreadAffinitySet = true;

	if (!cpu_mask)
	{
		return;
	}

	U32 vcpus = boost::thread::hardware_concurrency();
	U32 cores = boost::thread::physical_concurrency();
	if (vcpus < 4 || cores < 4)
	{
		llinfos << "Too few CPU cores to set an affinity. Skipping." << llendl;
		return;
	}

	U32 reserved_vcpus = 0;
# if LL_LINUX
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (int i = 0, last = llmin((int)CPU_SETSIZE, 32); i < last; ++i)
	{
		if (cpu_mask & (1 << i))
		{
			CPU_SET(i, &cpuset);
			++reserved_vcpus;
		}
	}
# else	// LL_WINDOWS
	// The mask is just a 64 bits value.
	DWORD_PTR cpuset = 0;
	for (int i = 0, last = sizeof(DWORD_PTR) * 8; i < last; ++i)
	{
		if (cpu_mask & (1 << i))
		{
			cpuset |= (1 << i);
			++reserved_vcpus;
		}
	}
# endif
	if (!reserved_vcpus)	// This should not happen (CPU_SETSIZE > 32)...
	{
		llwarns << "Request to reserve cores not part of available cores. Skipping."
				<< llendl;
		return;
	}
	if (reserved_vcpus + 2 > vcpus)
	{
		llwarns << "Request to reserve too many cores (" << reserved_vcpus
				<< ") for the main thread; only " << vcpus
				<< " cores are available on this system. Skipping." << llendl;
		return;
	}

# if LL_LINUX
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset))
# else	// LL_WINDOWS
	if (!SetThreadAffinityMask(GetCurrentThread(), cpuset))
# endif
	{
		llwarns << "Failed to set CPU affinity for the main thread." << llendl;
		return;
	}

	// Success !  Remember our mask so to set a complementary one on each new
	// child thread.
	sMainThreadAffinityMask = cpu_mask;
#endif	// LL_LINUX || LL_WINDOWS
}

//static
S32 LLCPUInfo::setThreadCPUAffinity(const char* name)
{
#if LL_LINUX || LL_WINDOWS
	if (!sMainThreadAffinitySet)
	{
		// Cannot set a child thread affinity before the main thread one is set
		if (name)
		{
			llwarns << "Cannot yet set CPU affinity for thread: " << name
					<< llendl;
		}
		return -1;
	}

	// This is a no-operation if no affinity setting is used, or when called
	// from the main thread. Report a "success" in both cases.
	if (!sMainThreadAffinityMask || is_main_thread())
	{
		return 1;
	}

# if LL_LINUX
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (U32 i = 0; i < (U32)CPU_SETSIZE; ++i)
	{
		if (i >= 32 || !(sMainThreadAffinityMask & (1 << i)))
		{
			CPU_SET((int)i, &cpuset);
		}
	}
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset))
# else	// LL_WINDOWS
	DWORD_PTR cpuset = 0;
	for (U32 i = 0; i < (U32)(sizeof(cpuset) * 8); ++i)
	{
		if (i >= 32 || !(sMainThreadAffinityMask & (1 << i)))
		{
			cpuset |= (1 << i);
		}
	}
	if (!SetThreadAffinityMask(GetCurrentThread(), cpuset)) 
# endif
	{
		if (name)
		{
			llwarns << "Failed to set CPU affinity for thread: " << name
					<< llendl;
		}
		return 0;
	}
#endif	// LL_LINUX || LL_WINDOWS
	return 1;
}

// Forward declaration (the function is moved at the end on purpose, so that we
// can disable the compiler optimizations for it, and for it only). HB
static F64 benchmark(U32 upper_limit);

// Returns the time needed (in ms) to find the prime numbers smaller than
// upper_limit, using the sieve of Eratosthenes algorithm (with all compiler
// optimizations turned off). Under Linux, also refreshes the CPU MHz info just
// after the benchmark (at which point, the affected CPU core should be running
// at max turbo frequency, which will also be used during rendering). HB
F32 LLCPUInfo::benchmarkFactor()
{
	// Reference values for benchmark(), as measured on a 9700K @ 5GHz (fixed,
	// locked frequency on all cores) under Linux (compiled with gcc, clang)
	// and Windows (compiled with MSVC). HB
	constexpr U32 BENCH_REF_LIMIT = 10000000;
#if LL_CLANG
	constexpr F32 BENCH_REF_9700K_5GHZ = 33.8f;	// In ms
#elif LL_GNUC
	constexpr F32 BENCH_REF_9700K_5GHZ = 29.8f;	// In ms
#else	// LL_MSVC
	constexpr F32 BENCH_REF_9700K_5GHZ = 31.f;	// In ms
#endif

	// Let's average several benchmarks for a total duration of 500ms to 1s.
	constexpr F64 MAX_DURATION = 500.0;					// In ms.
	// Let's not check the CPU/core frequency too frequently...
	constexpr F64 DELAY_BETWEEN_FREQ_CHECKS = 100.0;	// In ms.
	F64 total = 0.0;
	F64 last_freq_check = 0.0;
	F64 duration = 0.0;
	S32 iterations = 0;
	do
	{
		F64 sample = benchmark(BENCH_REF_LIMIT);
		total += sample;
		if (total - last_freq_check >= DELAY_BETWEEN_FREQ_CHECKS)
		{
			last_freq_check = total;
			if (affectedCoreFreqDelta() > 0 && total <= MAX_DURATION)
			{
				// Reject former results, since the CPU/core was not yet
				// in turbo mode... HB
				duration = 0.0;
				iterations = 0;
				continue;
			}
		}
		// Update the stats
		duration += sample;
		++iterations;
	}
	while (total <= MAX_DURATION);

	F32 result = F32(duration / F64(iterations));
	llinfos << "Time taken to find all prime numbers below " << BENCH_REF_LIMIT
			<< ": " << result << "ms";
	if (iterations > 1)
	{
		llcont << " (averaged on " << iterations << " runs)";
	}
	llcont << "." << llendl;

	F32 factor = BENCH_REF_9700K_5GHZ / result;
	llinfos << "CPU single-core performance factor relative to a 9700K @ 5GHz: "
			<< factor << llendl;

	return factor;
}

// Try and make the benchmark code performances incensitive to user-selected
// compiler optimizations, and *mostly* compiler-agnostic...
#if LL_CLANG
# pragma clang optimize off
#elif LL_GNUC
# pragma GCC optimize("O0")
#elif LL_MSVC
# pragma optimize("", off)
#endif

// This is a simple (integers-only) benchmark based on the well known sieve of
// Eratosthenes algorithm to find prime numbers. The algorithm itself is not
// optimized for primes-finding efficiency (aside from searching primes beyond
// 2 only among odd numbers). Its main advantage is that it benches only
// genuine CPU operations and not some standard C/C++ library function
// implementation. I tried to prevent some 'stealth' optimizations (i.e.
// optimizations done even at -O0) by some compilers via (minor) manual
// optimization of the code, e.g. by using intermediate variables for reusable
// results and replacing integer divisions and multiplications with bit
// shifting. Note however that the various compilers (or even the various
// compiler versions of a same compiler) generate a different code anyway, but
// all in all, and with optimizations off, they seem to perform almost equally
// with this implementation of the algorithm (see the BENCH_REF_9700K_5GHZ
// constexpr above for the measured figures). HB
static F64 benchmark(U32 upper_limit)
{
	if (upper_limit < 4)
	{
		return 0.0;
	}

	// Number of odd numbers between 3 and upper_limit;
	U32 odds = upper_limit / 2 - 1;
	// Avoids possible differences in compiler code generation (e.g. it could
	// be placed in a 64 bits register, or be converted (extended to 64 bits)
	// at each loop from upper_limit for the comparison with j)...
	U64 ul_64 = upper_limit;
	// An array of booleans (using a 'char' type instead of 'bool', to avoid
	// any potential optimization by compilers, such as using an integer type
	// when it is faster to access a 16 or 32 bits integer than a byte (char)
	// to represent a bool type on a specific CPU architecture); at the end of
	// the sieving process, an element will be true (non-zero) when its index
	// finally does not correspond to a prime number.
	char* non_prime_odds = new char[odds];
	memset((void*)non_prime_odds, 0, odds * sizeof(char));

	// Initialize and start the timer now (i.e. do not benchmark the C/C++
	// libraries implementations of new() and memset()).
	LLTimer timer;
	timer.start();

	// Check all odd numbers (2 * i + 3) between 3 and upper_limit
	for (U32 i = 0; i < odds; ++i)
	{
		if (!non_prime_odds[i])
		{
			U32 number = (i << 1) + 3;
			U32 k = number << 1;
			for (U64 j = k + number; j <= ul_64; j += k)
			{
				non_prime_odds[(j >> 1) - 1] = 1;
			}
		}
	}

	// End of timed benchmark
	F64 time = timer.getElapsedTimeF64() * 1000.0;	// Convert in ms

	delete[] non_prime_odds;

	return time;
}
