/***********************************************************************************************************************
*                                                                                                                      *
* ps6000d                                                                                                              *
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
	@author Andrew D. Zonenberg
	@brief Opening and closing the instrument (formerly the program entry point, main.cpp)
	@ingroup picobridge
 */

#ifdef HAS_PICO

#include "picobridge.h"
#include "PicoSCPIServer.h"

using namespace std;

namespace picobridge
{

/**
	@brief Opens the instrument and sets it up (formerly main())

	@param series	Model series to look for (2...6), or 0 for any
	@param serial	Serial number of the instrument to open, or empty for the first one found

	@return PICO_OK if the instrument was opened
 */
PICO_STATUS PicoSCPIServer::Open(unsigned int series, const string& serial)
{
	//Open the instrument with this serial number, or the first one we can find if it's empty
	int8_t* pserial = serial.empty() ? nullptr : (int8_t*)serial.c_str();
	m_series = series;
	PICO_INFO status = PICO_NOT_FOUND;
	switch(m_series)
	{
		case 0:
		{
			status = Open2000(pserial);
			if(status == 0)
				break;
			status = Open3000(pserial);
			if(status == 0)
				break;
			status = Open4000(pserial);
			if(status == 0)
				break;
			status = Open5000(pserial);
			if(status == 0)
				break;
			status = Open6000(pserial);
			break;
		}
		case 2:
		{
			status = Open2000(pserial);
			break;
		}
		case 3:
		{
			status = Open3000(pserial);
			break;
		}
		case 4:
		{
			status = Open4000(pserial);
			break;
		}
		case 5:
		{
			status = Open5000(pserial);
			break;
		}
		case 6:
		{
			status = Open6000(pserial);
			break;
		}
	}

	if(PICO_OK != status)
	{
		LogError("Failed to open unit (code 0x%x)\n", status);
		return status;
	}

	//See what we got
	{
		LogIndenter li;

		char buf[128];
		int16_t required = 0;
		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_DRIVER_VERSION);
		if(status == PICO_OK)
			LogVerbose("Driver version:   %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_USB_VERSION);
		if(status == PICO_OK)
			LogVerbose("USB version:      %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_HARDWARE_VERSION);
		if(status == PICO_OK)
			LogVerbose("Hardware version: %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_VARIANT_INFO);
		if(status == PICO_OK)
		{
			LogVerbose("Variant info:     %s\n", buf);
			m_model = buf;
		}

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_BATCH_AND_SERIAL);
		if(status == PICO_OK)
		{
			LogVerbose("Batch/serial:     %s\n", buf);
			m_serial = buf;
		}

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_CAL_DATE);
		if(status == PICO_OK)
			LogVerbose("Cal date:         %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_KERNEL_VERSION);
		if(status == PICO_OK)
			LogVerbose("Kernel ver:       %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_DIGITAL_HARDWARE_VERSION);
		if(status == PICO_OK)
			LogVerbose("Digital HW ver:   %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_ANALOGUE_HARDWARE_VERSION);
		if(status == PICO_OK)
			LogVerbose("Analog HW ver:    %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_FIRMWARE_VERSION_1);
		if(status == PICO_OK)
		{
			LogVerbose("FW ver 1:         %s\n", buf);
			m_fwver = buf;
		}

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_FIRMWARE_VERSION_2);
		if(status == PICO_OK)
			LogVerbose("FW ver 2:         %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_FIRMWARE_VERSION_3);
		if(status == PICO_OK)
			LogVerbose("FW ver 3:         %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_FRONT_PANEL_FIRMWARE_VERSION);
		if(status == PICO_OK)
			LogVerbose("Front panel FW:   %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_MAC_ADDRESS);
		if(status == PICO_OK)
			LogVerbose("MAC address:      %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_DRIVER_PATH);
		if(status == PICO_OK)
			LogVerbose("Driver path:      %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_SHADOW_CAL);
		if(status == PICO_OK)
			LogVerbose("Shadow cal:       %s\n", buf);

		status = m_picoGetUnitInfo(m_hScope, (int8_t*)buf, sizeof(buf), &required, PICO_IPP_VERSION);
		if(status == PICO_OK)
			LogVerbose("IPP version:      %s\n", buf);
	}
	LogNotice("Successfully opened instrument %s (%s)\n", m_model.c_str(), m_serial.c_str());

	//Limit to two channels only while on USB power
	if(m_limitChannels)
		m_numChannels = '2' - '0';
	else
		m_numChannels = m_model[1] - '0';

	//Initial channel state setup
	for(size_t i=0; i<m_numChannels; i++)
	{
		switch(m_pico_type)
		{
			case PICO2000A:
				m_api.ps2000aSetChannel(m_hScope, (PS2000A_CHANNEL)i, 0, PS2000A_DC, PS2000A_1V, 0.0f);
				break;
			case PICO3000A:
				m_api.ps3000aSetChannel(m_hScope, (PS3000A_CHANNEL)i, 0, PS3000A_DC, PS3000A_1V, 0.0f);
				break;
			case PICO4000A:
				m_api.ps4000aSetChannel(m_hScope, (PS4000A_CHANNEL)i, 0, PS4000A_DC, PICO_X1_PROBE_1V, 0.0f);
				break;
			case PICO5000A:
				m_api.ps5000aSetChannel(m_hScope, (PS5000A_CHANNEL)i, 0, PS5000A_DC, PS5000A_1V, 0.0f);
				break;
			case PICO6000A:
				m_api.ps6000aSetChannelOff(m_hScope, (PICO_CHANNEL)i);
				break;
			case PICOPSOSPA:
				m_api.psospaSetChannelOff(m_hScope, (PICO_CHANNEL)i);
				break;
		}
	}

	//Initialize analog channels
	for(size_t i=0; i<m_numChannels; i++)
	{
		m_channelOn[i] = false;
		m_coupling[i] = PICO_DC;
		m_range[i] = PICO_X1_PROBE_1V;
		m_range_psospa[i] = PICO_X1_PROBE_NV;
		m_range_2000a[i] = PS2000A_1V;
		m_range_3000e[i] = 1000000000;
		m_range_3000a[i] = PS3000A_1V;
		m_range_4000a[i] = PS4000A_1V;
		m_range_5000a[i] = PS5000A_1V;
		m_offset[i] = 0;
		m_bandwidth[i] = PICO_BW_FULL;
		m_bandwidth_3000a[i] = PS3000A_BW_FULL;
		m_bandwidth_4000a[i] = PS4000A_BW_FULL;
		m_bandwidth_5000a[i] = PS5000A_BW_FULL;
	}

	//Figure out digital channel configuration
	switch(m_series)
	{
		//No digital options for 4000 series.
		
		case 2:
		case 3:
		case 5:
			/* Model 3abcdMSO with
				a=4 chan, b=0(unknown) c=6(bandwidth 6=200MHz) d=D(revision D)
				MSO if MSO option is available
				example 3406DMSO (full option)
			*/
			/* Model 5abcdMSO with
				a=4 chan, b=4(flexres up to 16bit) c=4(bandwidth 4=200MHz) d=D(revision D)
				MSO if MSO option is available
				example 5444DMSO (full option)
			*/
			if(m_model.find("MSO") != string::npos)
			{
				m_numDigitalPods = 2;
			}
			else
			{
				m_numDigitalPods = 0;
			}
			break;

		case 6:
			m_numDigitalPods = 2;
			break;

		default:
			m_numDigitalPods = 0;
	}

	for(size_t i=0; i<m_numDigitalPods; i++)
	{
		m_msoPodEnabled[i] = false;
		for(size_t j=0; j<8; j++)
			m_msoPodThreshold[i][j] = 0;
		m_msoHysteresis[i] = PICO_NORMAL_100MV;
	}

	//Push initial trigger config
	UpdateTrigger();

	return PICO_OK;
}

///@brief Closes the instrument (formerly at the end of main())
void PicoSCPIServer::CloseUnit()
{
	if(m_hScope <= 0)
		return;

	switch(m_pico_type)
	{
		case PICO2000A:
			m_api.ps2000aCloseUnit(m_hScope);
			break;
		case PICO3000A:
			m_api.ps3000aCloseUnit(m_hScope);
			break;
		case PICO4000A:
			m_api.ps4000aCloseUnit(m_hScope);
			break;
		case PICO5000A:
			m_api.ps5000aCloseUnit(m_hScope);
			break;
		case PICO6000A:
			m_api.ps6000aCloseUnit(m_hScope);
			break;
		case PICOPSOSPA:
			m_api.psospaCloseUnit(m_hScope);
			break;
	}
	m_hScope = 0;
}

/**
	@brief Lists the instruments that are connected and not already open

	@return (serial number, description) of each
 */
vector<pair<string, string>> PicoSCPIServer::EnumerateUnits()
{
	vector<pair<string, string>> ret;
	auto& api = GetPicoApi();

	for(int i=0; i<PicoApi::NUM_FAMILIES; i++)
	{
		auto f = static_cast<PicoApi::Family>(i);
		if(!api.IsLoaded(f))
			continue;

		int16_t count = 0;
		char serials[1024] = {0};
		int16_t len = sizeof(serials) - 1;
		PICO_STATUS status = PICO_NOT_FOUND;
		switch(f)
		{
			case PicoApi::PS2000A:
				status = api.ps2000aEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			case PicoApi::PS3000A:
				status = api.ps3000aEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			case PicoApi::PS4000A:
				status = api.ps4000aEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			case PicoApi::PS5000A:
				status = api.ps5000aEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			case PicoApi::PS6000A:
				status = api.ps6000aEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			case PicoApi::PSOSPA:
				status = api.psospaEnumerateUnits(&count, (int8_t*)serials, &len);
				break;
			default:
				break;
		}
		if( (status != PICO_OK) || (count <= 0) )
			continue;

		//Comma separated list of serial numbers
		string s(serials, strnlen(serials, sizeof(serials)));
		size_t start = 0;
		while(start < s.length())
		{
			size_t end = s.find(',', start);
			if(end == string::npos)
				end = s.length();
			auto sn = Trim(s.substr(start, end - start));
			if(!sn.empty())
				ret.push_back(pair<string, string>(sn, string("PicoScope (") + PicoApi::GetFamilyName(f) + " API) " + sn));
			start = end + 1;
		}
	}

	return ret;
}

PICO_INFO PicoSCPIServer::Open2000(int8_t* serial)
{
	LogVerbose("Looking for a PicoScope 2000 series instrument to open...\n");
	if(!m_api.IsLoaded(PicoApi::PS2000A))
		return PICO_NOT_FOUND;
	PICO_INFO status = m_api.ps2000aOpenUnit(&m_hScope, serial);
	if(status == PICO_OK)
	{
		m_series = 2;
		m_pico_type = PICO2000A;
		m_picoGetUnitInfo = m_api.ps2000aGetUnitInfo;
	}
	return status;
}
PICO_INFO PicoSCPIServer::Open3000(int8_t* serial)
{
	LogVerbose("Looking for a PicoScope 3000 series instrument to open...\n");
	PICO_INFO status = PICO_NOT_FOUND;
	if(m_api.IsLoaded(PicoApi::PS3000A))
		status = m_api.ps3000aOpenUnit(&m_hScope, serial);
	if(status == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		// switch to USB power
		// TODO: maybe require the user to specify this is ok
		m_limitChannels = true;
		LogNotice("Switching to USB power...\n");
		status = m_api.ps3000aChangePowerSource(m_hScope, PICO_POWER_SUPPLY_NOT_CONNECTED);
	}
	if(status == PICO_OK)
	{
		m_series = 3;
		m_pico_type = PICO3000A;
		m_picoGetUnitInfo = m_api.ps3000aGetUnitInfo;
		return status;
	}

	if(!m_api.IsLoaded(PicoApi::PSOSPA))
		return status;
	status = m_api.psospaOpenUnit(&m_hScope, serial, PICO_DR_8BIT, NULL);
	if(status == PICO_OK)
	{
		m_series = 3;
		m_pico_type = PICOPSOSPA;
		m_picoGetUnitInfo = m_api.psospaGetUnitInfo;
	}
	return status;
}

PICO_INFO PicoSCPIServer::Open4000(int8_t* serial)
{
	LogVerbose("Looking for a PicoScope 4000 series instrument to open...\n");
	if(!m_api.IsLoaded(PicoApi::PS4000A))
		return PICO_NOT_FOUND;
	PICO_INFO status = m_api.ps4000aOpenUnit(&m_hScope, serial);
	if(status == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		// switch to USB power
		// only applies to model 4444
		m_limitChannels = true;
		LogNotice("Switching to USB power...\n");
		status = m_api.ps4000aChangePowerSource(m_hScope, PICO_POWER_SUPPLY_NOT_CONNECTED);
	}
	if(status == PICO_OK)
	{
		m_series = 4;
		m_pico_type = PICO4000A;
		m_picoGetUnitInfo = m_api.ps4000aGetUnitInfo;
	}
	return status;
}

PICO_INFO PicoSCPIServer::Open5000(int8_t* serial)
{
	LogVerbose("Looking for a PicoScope 5000 series instrument to open...\n");
	if(!m_api.IsLoaded(PicoApi::PS5000A))
		return PICO_NOT_FOUND;
	PICO_INFO status = m_api.ps5000aOpenUnit(&m_hScope, serial, PS5000A_DR_8BIT);
	if( (status == PICO_POWER_SUPPLY_NOT_CONNECTED) || (status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT) )
	{
		// switch to USB power
		// TODO: maybe require the user to specify this is ok
		m_limitChannels = true;
		LogNotice("Switching to USB power...\n");
		status = m_api.ps5000aChangePowerSource(m_hScope, PICO_POWER_SUPPLY_NOT_CONNECTED);
	}
	if(status == PICO_OK)
	{
		m_series = 5;
		m_pico_type = PICO5000A;
		m_picoGetUnitInfo = m_api.ps5000aGetUnitInfo;
	}
	return status;
}
PICO_INFO PicoSCPIServer::Open6000(int8_t* serial)
{
	LogVerbose("Looking for a PicoScope 6000 series instrument to open...\n");
	if(!m_api.IsLoaded(PicoApi::PS6000A))
		return PICO_NOT_FOUND;
	PICO_INFO status = m_api.ps6000aOpenUnit(&m_hScope, serial, PICO_DR_8BIT);
	if(status == PICO_OK)
	{
		m_series = 6;
		m_pico_type = PICO6000A;
		m_picoGetUnitInfo = m_api.ps6000aGetUnitInfo;
	}
	return status;
}

}

#endif
