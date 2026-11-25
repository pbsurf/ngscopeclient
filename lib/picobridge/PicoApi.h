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
	@brief Declaration of PicoApi, the table of Pico SDK functions loaded at run time
	@ingroup picobridge
 */

#ifndef PicoApi_h
#define PicoApi_h

#include "PicoSDK.h"

#include <string>

namespace picobridge
{

/**
	@brief Pointers to the Pico SDK functions, loaded at run time

	The SDK libraries (ps6000a.dll, libps6000a.so, ...) are loaded the first time GetPicoApi() is called, so
	ngscopeclient builds without the SDK's import libraries and runs on machines without the SDK. Each API family is
	usable only if its library loaded and has every function in PicoApiFunctions.h; otherwise its pointers are null.

	The members have the same names as the SDK functions, so a bridge call looks like m_api.ps6000aRunBlock(...).

	@ingroup picobridge
 */
class PicoApi
{
public:
	///@brief The Pico "A" APIs, one library each
	enum Family
	{
		PS2000A,
		PS3000A,
		PS4000A,
		PS5000A,
		PS6000A,
		PSOSPA,

		NUM_FAMILIES
	};

	PicoApi();

	//not copyable or assignable
	PicoApi(const PicoApi&) =delete;
	PicoApi& operator=(const PicoApi&) =delete;

	///@brief Returns true if the library for this API family loaded with all the functions we need
	bool IsLoaded(Family f) const
	{ return m_loaded[f]; }

	bool IsAnyLoaded() const;

	static const char* GetFamilyName(Family f);

	#define PICO_FN(api, name) decltype(&::name) name = nullptr;
	#include "PicoApiFunctions.h"
	#undef PICO_FN

protected:
	void Load(Family f);
	void* OpenLibrary(Family f);
	void* GetSymbol(void* lib, const char* name);

	///@brief True if the library for each family loaded completely
	bool m_loaded[NUM_FAMILIES];
};

PicoApi& GetPicoApi();

}

#endif
