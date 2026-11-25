/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author ngscopeclient contributors
	@brief Implementation of PicoApi
	@ingroup picobridge
 */

#ifdef HAS_PICO

#include "picobridge.h"
#include "PicoApi.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace std;

namespace picobridge
{

/**
	@brief Gets the Pico SDK function table, loading the SDK libraries on the first call
 */
PicoApi& GetPicoApi()
{
	static PicoApi api;
	return api;
}

PicoApi::PicoApi()
{
	LogDebug("Loading Pico SDK libraries\n");
	LogIndenter li;

	for(int i=0; i<NUM_FAMILIES; i++)
		Load(static_cast<Family>(i));

	if(!IsAnyLoaded())
		LogDebug("No Pico SDK libraries found, PicoScope support will not be available\n");
}

bool PicoApi::IsAnyLoaded() const
{
	for(auto b : m_loaded)
	{
		if(b)
			return true;
	}
	return false;
}

///@brief Returns the library base name of an API family ("ps6000a")
const char* PicoApi::GetFamilyName(Family f)
{
	switch(f)
	{
		case PS2000A:	return "ps2000a";
		case PS3000A:	return "ps3000a";
		case PS4000A:	return "ps4000a";
		case PS5000A:	return "ps5000a";
		case PS6000A:	return "ps6000a";
		case PSOSPA:	return "psospa";
		default:		return "";
	}
}

/**
	@brief Loads one API family's library and looks up its functions

	If the library or any of its functions is missing, all of the family's pointers are left null.
 */
void PicoApi::Load(Family f)
{
	m_loaded[f] = false;

	auto lib = OpenLibrary(f);
	if(!lib)
		return;

	//Look up every function, then clear them all again if any is missing
	bool ok = true;
	#define PICO_FN(api, name) \
		if(api == f) \
		{ \
			name = reinterpret_cast<decltype(name)>(GetSymbol(lib, #name)); \
			if(!name) \
			{ \
				LogWarning("Pico SDK library %s has no function %s, %s scopes will not be available\n", \
					GetFamilyName(f), #name, GetFamilyName(f)); \
				ok = false; \
			} \
		}
	#include "PicoApiFunctions.h"
	#undef PICO_FN

	if(!ok)
	{
		#define PICO_FN(api, name) \
			if(api == f) \
				name = nullptr;
		#include "PicoApiFunctions.h"
		#undef PICO_FN
		return;
	}

	m_loaded[f] = true;
}

#ifdef _WIN32

/**
	@brief Opens an API family's library

	Looks in the ngscopeclient directory first, then in the Pico SDK's default install directory, then in the
	standard DLL search path. The SDK libraries depend on other DLLs in their directory (picoipp.dll), so a library
	found by full path is loaded with LOAD_WITH_ALTERED_SEARCH_PATH to find those there too.
 */
void* PicoApi::OpenLibrary(Family f)
{
	string fname = string(GetFamilyName(f)) + ".dll";

	vector<string> paths;
	char exePath[MAX_PATH] = {0};
	DWORD n = GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
	if( (n > 0) && (n < sizeof(exePath)) )
	{
		string binDir(exePath);
		auto slash = binDir.find_last_of("\\/");
		if(slash != string::npos)
			paths.push_back(binDir.substr(0, slash + 1) + fname);
	}
	paths.push_back("C:\\Program Files\\Pico Technology\\SDK\\lib\\" + fname);

	//Don't pop up a dialog if a dependency is missing
	DWORD oldMode = 0;
	SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &oldMode);

	HMODULE lib = nullptr;
	for(auto& path : paths)
	{
		if(GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue;
		lib = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if(lib)
		{
			LogDebug("Loaded %s\n", path.c_str());
			break;
		}
		LogWarning("Found %s but could not load it (error %lu)\n", path.c_str(), GetLastError());
	}
	if(!lib)
	{
		lib = LoadLibraryA(fname.c_str());
		if(lib)
			LogDebug("Loaded %s from the DLL search path\n", fname.c_str());
	}

	SetThreadErrorMode(oldMode, nullptr);
	return reinterpret_cast<void*>(lib);
}

void* PicoApi::GetSymbol(void* lib, const char* name)
{
	return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(lib), name));
}

#else

/**
	@brief Opens an API family's library

	Looks in the standard library search path first, then in the Pico SDK's default install directory.
 */
void* PicoApi::OpenLibrary(Family f)
{
	string base = GetFamilyName(f);

	vector<string> paths;
#ifdef __APPLE__
	paths.push_back("lib" + base + ".dylib");
	paths.push_back("/Library/Frameworks/PicoSDK.framework/Libraries/lib" + base + "/lib" + base + ".dylib");
#else
	paths.push_back("lib" + base + ".so");
	paths.push_back("lib" + base + ".so.2");
	paths.push_back("/opt/picoscope/lib/lib" + base + ".so");
#endif

	for(auto& path : paths)
	{
		void* lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
		if(lib)
		{
			LogDebug("Loaded %s\n", path.c_str());
			return lib;
		}
	}
	return nullptr;
}

void* PicoApi::GetSymbol(void* lib, const char* name)
{
	return dlsym(lib, name);
}

#endif

}

#endif
