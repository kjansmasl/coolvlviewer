/**
 * @file lldxhardware.cpp
 * @brief LLDXHardware implementation
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

#ifdef LL_WINDOWS

// Culled from some Microsoft sample code

#include "linden_common.h"

#include <combaseapi.h>				// For IID_PPV_ARGS()
#define INITGUID
#include <dxdiag.h>
#undef INITGUID
#include <dxgi.h>
#include <stdlib.h>					// For getenv()
#include <wbemidl.h>

#include "lldxhardware.h"

void (*gWriteDebug)(const char* msg) = NULL;
LLDXHardware gDXHardware;

#define SAFE_RELEASE(p)	{ if (p) { (p)->Release(); (p)=NULL; } }

static void get_wstring(IDxDiagContainer* containerp, WCHAR* prop_name,
						WCHAR* prop_value, int output_size)
{
	VARIANT var;
	VariantInit(&var);
	HRESULT hr = containerp->GetProp(prop_name, &var);
	if (SUCCEEDED(hr))
	{
		// Switch off the type.  There's 4 different types:
		switch (var.vt)
		{
			case VT_UI4:
				swprintf(prop_value, L"%d", var.ulVal);
				break;
			case VT_I4:
				swprintf(prop_value, L"%d", var.lVal);
				break;
			case VT_BOOL:
				wcscpy(prop_value, var.boolVal ? L"true" : L"false");
				break;
			case VT_BSTR:
				wcsncpy(prop_value, var.bstrVal, output_size - 1);
				prop_value[output_size - 1] = 0;
				break;
		}
	}
	// Clear the variant (this is needed to free BSTR memory)
	VariantClear(&var);
}

static std::string get_string(IDxDiagContainer* containerp, WCHAR* prop_name)
{
	WCHAR prop_value[256];
	get_wstring(containerp, prop_name, prop_value, 256);
	return ll_convert_wide_to_string(prop_value);
}

//static
S32 LLDXHardware::getMBVideoMemoryViaDXGI()
{
	// Let the user override the detection in case it fails on their system.
	// They can specify the amount of VRAM in megabytes, via the LL_VRAM_MB
	// environment variable. HB
	char* vram_override = getenv("LL_VRAM_MB");
	if (vram_override)
	{
		S32 vram = atoi(vram_override);
		if (vram > 0)
		{
			llinfos << "Amount of VRAM overridden via the LL_VRAM_MB environment variable; detection step skipped. VRAM amount: "
					<< vram << "MB" << llendl;
			return vram;
		}
	}

	SIZE_T vram = 0;
	if (SUCCEEDED(CoInitialize(0)))
	{
		IDXGIFactory1* factoryp = NULL;
		HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factoryp));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter1* adapterp = NULL;
			IDXGIAdapter1* tmp_adapterp = NULL;
			DXGI_ADAPTER_DESC1 desc;
			UINT idx = 0;
			while (factoryp->EnumAdapters1(idx++, &tmp_adapterp) !=
					DXGI_ERROR_NOT_FOUND)
			{
				if (!tmp_adapterp)	// Should not happen.
				{
					break;
				}
				hr = tmp_adapterp->GetDesc1(&desc);
				if (SUCCEEDED(hr) && desc.Flags == 0)
				{
					tmp_adapterp->QueryInterface(IID_PPV_ARGS(&adapterp));
					if (adapterp)
					{
						adapterp->GetDesc1(&desc);
						if (desc.DedicatedVideoMemory > vram)
						{
							vram = desc.DedicatedVideoMemory;
						}
						SAFE_RELEASE(adapterp);
					}
				}
				SAFE_RELEASE(tmp_adapterp);
			}
			SAFE_RELEASE(factoryp);
		}
		CoUninitialize();
	}
	return vram / (1024 * 1024);
}

LLSD LLDXHardware::getDisplayInfo()
{
	if (mInfo.size())
	{
		return mInfo;
	}

	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		llwarns << "COM library initialization failed !" << llendl;
		gWriteDebug("COM library initialization failed !\n");
		return mInfo;
	}

	IDxDiagProvider* dx_diag_providerp = NULL;
	IDxDiagContainer* dx_diag_rootp = NULL;
	IDxDiagContainer* devices_containerp = NULL;
	IDxDiagContainer* device_containerp = NULL;
	IDxDiagContainer* file_containerp = NULL;
	IDxDiagContainer* driver_containerp = NULL;

	// CoCreate a IDxDiagProvider*
	llinfos << "CoCreateInstance IID_IDxDiagProvider" << llendl;
	hr = CoCreateInstance(CLSID_DxDiagProvider, NULL, CLSCTX_INPROC_SERVER,
						  IID_IDxDiagProvider, (LPVOID*)&dx_diag_providerp);
	if (FAILED(hr))
	{
		llwarns << "No DXDiag provider found !  DirectX not installed !"
				<< llendl;
		gWriteDebug("No DXDiag provider found !  DirectX not installed !\n");
		goto exit_cleanup;
	}
	if (SUCCEEDED(hr)) // if FAILED(hr) then dx9 is not installed
	{
		// Fill out a DXDIAG_INIT_PARAMS struct and pass it to
		// IDxDiagContainer::Initialize(). Passing in TRUE for bAllowWHQLChecks
		// allows dxdiag to check if drivers are digital signed as logo'd by
		// WHQL which may connect via internet to update WHQL certificates.
		DXDIAG_INIT_PARAMS dx_diag_init_params;
		ZeroMemory(&dx_diag_init_params, sizeof(DXDIAG_INIT_PARAMS));

		dx_diag_init_params.dwSize = sizeof(DXDIAG_INIT_PARAMS);
		dx_diag_init_params.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
		dx_diag_init_params.bAllowWHQLChecks = TRUE;
		dx_diag_init_params.pReserved = NULL;

		LL_DEBUGS("AppInit") << "dx_diag_providerp->Initialize" << LL_ENDL;
		hr = dx_diag_providerp->Initialize(&dx_diag_init_params);
		if (FAILED(hr))
		{
			goto exit_cleanup;
		}

		LL_DEBUGS("AppInit") << "dx_diag_providerp->GetRootContainer"
							 << LL_ENDL;
		hr = dx_diag_providerp->GetRootContainer(&dx_diag_rootp);
		if (FAILED(hr) || !dx_diag_rootp)
		{
			goto exit_cleanup;
		}

		HRESULT hr;

		// Get display driver information
		LL_DEBUGS("AppInit") << "dx_diag_rootp->GetChildContainer" << LL_ENDL;
		hr = dx_diag_rootp->GetChildContainer(L"DxDiag_DisplayDevices",
												&devices_containerp);
		if (FAILED(hr) || !devices_containerp)
		{
			// Do not release 'dirty' devices_containerp at this stage, only
			// dx_diag_rootp
			devices_containerp = NULL;
			goto exit_cleanup;
		}

		DWORD dw_device_count;
		// Make sure there is something inside
		hr = devices_containerp->GetNumberOfChildContainers(&dw_device_count);
		if (FAILED(hr) || dw_device_count == 0)
		{
			goto exit_cleanup;
		}

		// Get device 0
		LL_DEBUGS("AppInit") << "devices_containerp->GetChildContainer"
							 << LL_ENDL;
		hr = devices_containerp->GetChildContainer(L"0", &device_containerp);
		if (FAILED(hr) || !device_containerp)
		{
			goto exit_cleanup;
		}

		// Get the English VRAM string
		std::string ram_str = get_string(device_containerp,
										 L"szDisplayMemoryEnglish");

		// Dump the string as an int into the structure
		char* stopstring;
		mInfo["VRAM"] = S32(strtol(ram_str.c_str(),
								   &stopstring, 10) / (1024 * 1024));
		std::string device_name = get_string(device_containerp,
											 L"szDescription");
		mInfo["DeviceName"] = device_name;
		std::string device_driver=  get_string(device_containerp,
											   L"szDriverVersion");
		mInfo["DriverVersion"] = device_driver;

		// ATI has a slightly different version string
		if (device_name.length() >= 4 && device_name.substr(0, 4) == "ATI ")
		{
			// Get the key
			HKEY hKey;
			const DWORD RV_SIZE = 100;
			WCHAR release_version[RV_SIZE];

			// Hard coded registry entry. Using this since it is simpler for
			// now. And using EnumDisplayDevices to get a registry key also
			// requires a hard coded Query value.
			if (ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE,
											TEXT("SOFTWARE\\ATI Technologies\\CBT"),
											&hKey))
			{
				// Get the value
				DWORD dwType = REG_SZ;
				DWORD dwSize = sizeof(WCHAR) * RV_SIZE;
				if (ERROR_SUCCESS == RegQueryValueEx(hKey,
													 TEXT("ReleaseVersion"),
				 									 NULL, &dwType,
													 (LPBYTE)release_version,
													 &dwSize))
				{
					// Print the value; Windows does not guarantee to be nul
					// terminated
					release_version[RV_SIZE - 1] = 0;
					mInfo["DriverVersion"] =
						ll_convert_wide_to_string(release_version);

				}
				RegCloseKey(hKey);
			}
		}
	}

exit_cleanup:
	if (!mInfo.size())
	{
		llinfos << "Failed to get data, cleaning up..." << llendl;
	}
	SAFE_RELEASE(file_containerp);
	SAFE_RELEASE(driver_containerp);
	SAFE_RELEASE(device_containerp);
	SAFE_RELEASE(devices_containerp);
	SAFE_RELEASE(dx_diag_rootp);
	SAFE_RELEASE(dx_diag_providerp);

	CoUninitialize();
	return mInfo;
}

#endif
