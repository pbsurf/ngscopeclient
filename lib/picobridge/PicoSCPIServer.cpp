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
	@brief SCPI server. Control plane traffic only, no waveform data.

	SCPI commands supported:

		*IDN?
			Returns a standard SCPI instrument identification string

		CHANS?
			Returns the number of channels on the instrument.

		SEQNUM?
			Returns the most recently sent sequence number

		[1|2]D:PRESENT?
			Returns 1 = MSO pod present, 0 = MSO pod not present

		[chan]:BWLIM [freq]
			Sets the channel's bandwith limiter to freq in MHz, 0 for full bandwidth.

		[chan]:BWLIM?
			Returns the channel's bandwith limiter frequency in MHz, 0 for full bandwidth.

		[chan]:COUP [DC1M|AC1M|DC50]
			Sets channel coupling

		[chan]:HYS [mV]
			Sets MSO channel hysteresis to mV millivolts

		[chan]:OFF
			Turns the channel off

		[chan]:OFFS [num]
			Sets channel offset to num volts

		[chan]:ON
			Turns the channel on

		[chan]:RANGE [num]
			Sets channel full-scale range to num volts

		[chan]:THRESH [mV]
			Sets MSO channel threshold to mV millivolts

		BITS [num]
			Sets ADC bit depth

		DEPTH [num]
			Sets memory depth

		DEPTHS?
			Returns the set of available memory depths

		EXIT
			Terminates the connection

		FORCE
			Forces a single acquisition

		RATE [num]
			Sets sample rate

		RATES?
			Returns a comma separated list of sampling rates (in femtoseconds)

		SINGLE
			Arms the trigger in one-shot mode

		START
			Arms the trigger

		STOP
			Disarms the trigger

		TRIG:DELAY [delay]
			Sets trigger delay (in fs)

		TRIG:EDGE:DIR [direction]
			Sets trigger direction. Legal values are RISING, FALLING, or ANY.

		TRIG:LEV [level]
			Selects trigger level (in volts)

		TRIG:SOU [chan]
			Selects the channel as the trigger source

		TODO: SetDigitalPortInteractionCallback to determine when pods are connected/removed

		AWG:DUTY [duty cycle]
			Sets duty cycle of function generator output

		AWG:FREQ [freq]
			Sets function generator frequency, in Hz

		AWG:OFF [offset]
			Sets offset of the function generator output

		AWG:RANGE [range]
			Sets p-p voltage of the function generator output

		AWG:SHAPE [waveform type]
			Sets waveform type

		AWG:START
			Starts the function generator

		AWG:STOP
			Stops the function generator
 */

#ifdef HAS_PICO

#include "picobridge.h"
#include "PicoSCPIServer.h"
#include <string.h>
#include <math.h>

using namespace std;

namespace picobridge
{

//Struct easily allows for adding new models
struct WaveformType
{
	PICO_WAVE_TYPE type6000;	//6000E and PSOSPA
	PS2000A_WAVE_TYPE type2000;
	PS2000A_EXTRA_OPERATIONS op2000;
	PS3000A_WAVE_TYPE type3000;
	PS3000A_EXTRA_OPERATIONS op3000;
	PS4000A_WAVE_TYPE type4000;
	PS4000A_EXTRA_OPERATIONS op4000;
	PS5000A_WAVE_TYPE type5000;
	PS5000A_EXTRA_OPERATIONS op5000;
};
static const map<string, WaveformType> g_waveformTypes =
{
	{"SINE",       {PICO_SINE,       PS2000A_SINE,           PS2000A_ES_OFF,     PS3000A_SINE,           PS3000A_ES_OFF,     PS4000A_SINE,           PS4000A_ES_OFF,     PS5000A_SINE,            PS5000A_ES_OFF}},
	{"SQUARE",     {PICO_SQUARE,     PS2000A_SQUARE,         PS2000A_ES_OFF,     PS3000A_SQUARE,         PS3000A_ES_OFF,     PS4000A_SQUARE,         PS4000A_ES_OFF,     PS5000A_SQUARE,          PS5000A_ES_OFF}},
	{"TRIANGLE",   {PICO_TRIANGLE,   PS2000A_TRIANGLE,       PS2000A_ES_OFF,     PS3000A_TRIANGLE,       PS3000A_ES_OFF,     PS4000A_TRIANGLE,       PS4000A_ES_OFF,     PS5000A_TRIANGLE,        PS5000A_ES_OFF}},
	{"RAMP_UP",    {PICO_RAMP_UP,    PS2000A_RAMP_UP,        PS2000A_ES_OFF,     PS3000A_RAMP_UP,        PS3000A_ES_OFF,     PS4000A_RAMP_UP,        PS4000A_ES_OFF,     PS5000A_RAMP_UP,         PS5000A_ES_OFF}},
	{"RAMP_DOWN",  {PICO_RAMP_DOWN,  PS2000A_RAMP_DOWN,      PS2000A_ES_OFF,     PS3000A_RAMP_DOWN,      PS3000A_ES_OFF,     PS4000A_RAMP_DOWN,      PS4000A_ES_OFF,     PS5000A_RAMP_DOWN,       PS5000A_ES_OFF}},
	{"SINC",       {PICO_SINC,       PS2000A_SINC,           PS2000A_ES_OFF,     PS3000A_SINC,           PS3000A_ES_OFF,     PS4000A_SINC,           PS4000A_ES_OFF,     PS5000A_SINC,            PS5000A_ES_OFF}},
	{"GAUSSIAN",   {PICO_GAUSSIAN,   PS2000A_GAUSSIAN,       PS2000A_ES_OFF,     PS3000A_GAUSSIAN,       PS3000A_ES_OFF,     PS4000A_GAUSSIAN,       PS4000A_ES_OFF,     PS5000A_GAUSSIAN,        PS5000A_ES_OFF}},
	{"HALF_SINE",  {PICO_HALF_SINE,  PS2000A_HALF_SINE,      PS2000A_ES_OFF,     PS3000A_HALF_SINE,      PS3000A_ES_OFF,     PS4000A_HALF_SINE,      PS4000A_ES_OFF,     PS5000A_HALF_SINE,       PS5000A_ES_OFF}},
	{"DC",         {PICO_DC_VOLTAGE, PS2000A_DC_VOLTAGE,     PS2000A_ES_OFF,     PS3000A_DC_VOLTAGE,     PS3000A_ES_OFF,     PS4000A_DC_VOLTAGE,     PS4000A_ES_OFF,     PS5000A_DC_VOLTAGE,      PS5000A_ES_OFF}},
	{"WHITENOISE", {PICO_WHITENOISE, PS2000A_SINE,           PS2000A_WHITENOISE, PS3000A_SINE,           PS3000A_WHITENOISE, PS4000A_SINE,           PS4000A_WHITENOISE, PS5000A_SINE,            PS5000A_WHITENOISE}},
	{"PRBS",       {PICO_PRBS,       PS2000A_SINE,           PS2000A_PRBS,       PS3000A_SINE,           PS3000A_PRBS,       PS4000A_SINE,           PS4000A_PRBS,       PS5000A_SINE,            PS5000A_PRBS  }},
	{"ARBITRARY",  {PICO_ARBITRARY,  PS2000A_MAX_WAVE_TYPES, PS2000A_ES_OFF,     PS3000A_MAX_WAVE_TYPES, PS3000A_ES_OFF,     PS4000A_MAX_WAVE_TYPES, PS4000A_ES_OFF,     PS5000A_MAX_WAVE_TYPES,  PS5000A_ES_OFF}}       //FIX: PS3000A_MAX_WAVE_TYPES is used as placeholder for arbitrary generation till a better workaround is found
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PicoSCPIServer::PicoSCPIServer()
	: m_api(GetPicoApi())
	, m_clientStarted(false)
	, m_waveformThreadQuit(false)
{
}

/**
	@brief Sets up for a client after Open(), and starts the waveform thread

	Formerly the PicoSCPIServer constructor plus the client connection loop in main().
 */
void PicoSCPIServer::StartClient()
{
	m_roundedRange[PICO_TRIGGER_AUX] = 1;
	m_offset[PICO_TRIGGER_AUX] = 0;

	//set model dependent AWG buffer size and external trigger input voltage range
	switch(m_series)
	{
		case 3:
		{
			m_roundedRange[PICO_TRIGGER_AUX] = 5;
			m_awgBufferSize = 32768;
			if( (m_model.find("06A") != string::npos) || (m_model.find("06B") != string::npos) )
				m_awgBufferSize = 16384;
			if( (m_model.find("05A") != string::npos) || (m_model.find("05B") != string::npos) )
				m_awgBufferSize = 8192;
			if( (m_model.find("04A") != string::npos) || (m_model.find("04B") != string::npos) )
				m_awgBufferSize = 8192;
			break;
		}
		case 4:
		{
			m_awgBufferSize = 16384;
			break;
		}
		case 5:
		{
			m_roundedRange[PICO_TRIGGER_AUX] = 5;
			m_awgBufferSize = 32768;
			if(m_model.find("42B") != string::npos)
				m_awgBufferSize = 16384;
			if(m_model.find("44B") != string::npos)
				m_awgBufferSize = 49152;
			break;
		}
		case 6:
		{
			m_roundedRange[PICO_TRIGGER_AUX] = 1;
			m_awgBufferSize = 40960;
			break;
		}
	}
	delete[] m_arbitraryWaveform;
	m_arbitraryWaveform = new int16_t[m_awgBufferSize];

	//Launch the data-plane thread
	m_waveformThreadQuit = false;
	m_dataPipe.Reopen();
	m_ackPipe.Reopen();
	m_waveformThread = thread(&PicoSCPIServer::WaveformServerThread, this);
	m_clientStarted = true;
}

PicoSCPIServer::~PicoSCPIServer()
{
	StopClient();

	//Formerly done at the end of main()
	CloseUnit();
}

/**
	@brief Cleans up after a client, stopping the waveform thread (formerly the PicoSCPIServer destructor)

	The instrument stays open, ready for StartClient() again.
 */
void PicoSCPIServer::StopClient()
{
	if(!m_clientStarted)
		return;
	m_clientStarted = false;

	//Stop the waveform thread (formerly done in main() when the client disconnected)
	m_waveformThreadQuit = true;
	m_dataPipe.Close();
	m_ackPipe.Close();
	if(m_waveformThread.joinable())
		m_waveformThread.join();

	LogVerbose("Client disconnected\n");

	//Disable all channels when a client disconnects to put the scope in a "safe" state
	for(auto& it : m_channelOn)
	{
		switch(m_pico_type)
		{
			case PICO2000A:
				m_api.ps2000aSetChannel(m_hScope, (PS2000A_CHANNEL)it.first, 0, PS2000A_DC, PS2000A_1V, 0.0f);
				break;
			case PICO3000A:
				m_api.ps3000aSetChannel(m_hScope, (PS3000A_CHANNEL)it.first, 0, PS3000A_DC, PS3000A_1V, 0.0f);
				break;
			case PICO4000A:
				m_api.ps4000aSetChannel(m_hScope, (PS4000A_CHANNEL)it.first, 0, PS4000A_DC, PICO_X1_PROBE_1V, 0.0f);
				break;
			case PICO5000A:
				m_api.ps5000aSetChannel(m_hScope, (PS5000A_CHANNEL)it.first, 0, PS5000A_DC, PS5000A_1V, 0.0f);
				break;
			case PICO6000A:
				m_api.ps6000aSetChannelOff(m_hScope, (PICO_CHANNEL)it.first);
				break;
			case PICOPSOSPA:
				m_api.psospaSetChannelOff(m_hScope, (PICO_CHANNEL)it.first);
				break;
		}

		it.second = false;
		m_channelOnDuringArm[it.first] = false;
	}

	for(int i=0; i<2; i++)
	{
		switch(m_pico_type)
		{
			case PICO2000A:
				m_api.ps2000aSetDigitalPort(m_hScope, (PS2000A_DIGITAL_PORT)(PICO_PORT0 + i), 0, 0);
				break;
			case PICO3000A:
				m_api.ps3000aSetDigitalPort(m_hScope, (PS3000A_DIGITAL_PORT)(PICO_PORT0 + i), 0, 0);
				break;
			case PICO4000A:
				//no digital ports in series 4000
				break;
			case PICO5000A:
				m_api.ps5000aSetDigitalPort(m_hScope, (PS5000A_CHANNEL)(PICO_PORT0 + i), 0, 0);
				break;
			case PICO6000A:
				m_api.ps6000aSetDigitalPortOff(m_hScope, (PICO_CHANNEL)(PICO_PORT0 + i));
				break;
			case PICOPSOSPA:
				m_api.psospaSetDigitalPortOff(m_hScope, (PICO_CHANNEL)(PICO_PORT0 + i));
				break;
		}
		m_msoPodEnabled[i] = false;
	}

	delete[] m_arbitraryWaveform;
	m_arbitraryWaveform = nullptr;
}

/**
	@brief Closes the instrument when the process is told to quit (formerly OnQuit() in main.cpp)

	Doesn't wait for the waveform thread, since the process is about to end anyway.
 */
void PicoSCPIServer::Shutdown()
{
	LogNotice("Shutting down...\n");

	lock_guard<mutex> lock(m_mutex);
	CloseUnit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command parsing

bool PicoSCPIServer::OnQuery(
	const string& line,
	const string& subject,
	const string& cmd)
{
	//SEQNUM?
	if(cmd == "SEQNUM")
	{
		SendReply(to_string(m_lastTxSeq));
		return true;
	}

	//Extract channel ID from subject and clamp bounds
	size_t channelId = 0;
	//size_t laneId = 0;
	//bool channelIsDigital = false;
	if(isalpha(subject[0]))
	{
		channelId = min(static_cast<size_t>(subject[0] - 'A'), m_numChannels);
		//channelIsDigital = false;
	}
	else if(isdigit(subject[0]))
	{
		channelId = min(subject[0] - '0', 2) - 1;
		//channelIsDigital = true;
		//if(subject.length() >= 3)
		//	laneId = min(subject[2] - '0', 7);
	}

	if(BridgeSCPIServer::OnQuery(line, subject, cmd))
	{
		return true;
	}
	else if(cmd == "PRESENT")
	{
		lock_guard<mutex> lock(m_mutex);

		switch(m_series)
		{
			case 2:
			case 3:
			case 5:
			{
				//All MSO models have two pods.
				if(m_model.find("MSO") != string::npos)
				{
					SendReply("1");
				}
				else
				{
					SendReply("0");
				}
				break;
			}
			break;

			case 6:
			{
				//There's no API to test for presence of a MSO pod without trying to enable it.
				//If no pod is present, this call will return PICO_NO_MSO_POD_CONNECTED.
				PICO_CHANNEL podId = (PICO_CHANNEL)(PICO_PORT0 + channelId);
				auto status = m_api.ps6000aSetDigitalPortOn(
								  m_hScope,
								  podId,
								  m_msoPodThreshold[channelId],
								  8,
								  m_msoHysteresis[channelId]);

				if(status == PICO_NO_MSO_POD_CONNECTED)
				{
					SendReply("0");
				}
				else
				{
					// The pod is here. If we don't need it on, shut it back off
					if(!m_msoPodEnabled[channelId])
						m_api.ps6000aSetDigitalPortOff(m_hScope, podId);

					SendReply("1");
				}
			}
			break;

			default:
			{
				SendReply("0");
			}
		}
	}

	else if(cmd == "BWLIM")
	{
		lock_guard<mutex> lock(m_mutex);
		string ret = "0";

		switch(m_pico_type)
		{
			case PICO2000A:
				//no limiter available
				break;
			case PICO3000A:
				if(m_bandwidth_3000a[channelId] == PS3000A_BW_20MHZ)
					ret = "20";
				break;
			case PICO4000A:
				if(m_bandwidth_4000a[channelId] == PS4000A_BW_1MHZ)
					ret = "1";
				break;
			case PICO5000A:
				if(m_bandwidth_5000a[channelId] == PS5000A_BW_20MHZ)
					ret = "20";
				break;
			case PICO6000A:
				if(m_bandwidth[channelId] == PICO_BW_20MHZ)
					ret = "20";
				else if(m_bandwidth[channelId] == PICO_BW_200MHZ)
					ret = "200";
				break;
			case PICOPSOSPA:
				if(m_bandwidth[channelId] == PICO_BW_20MHZ)
					ret = "20";
				else if(m_bandwidth[channelId] == PICO_BW_50MHZ)
					ret = "50";
				else if(m_bandwidth[channelId] == PICO_BW_100MHZ)
					ret = "100";
				else if(m_bandwidth[channelId] == PICO_BW_200MHZ)
					ret = "200";
				else if(m_bandwidth[channelId] == PICO_BW_350MHZ)
					ret = "350";
				else if(m_bandwidth[channelId] == PICO_BW_500MHZ)
					ret = "500";
				break;
		}
		SendReply(ret);
	}

	else
	{
		LogDebug("Unrecognized query received: %s\n", line.c_str());
	}
	return false;
}

string PicoSCPIServer::GetMake()
{
	return "Pico Technology";
}

string PicoSCPIServer::GetModel()
{
	return m_model;
}

string PicoSCPIServer::GetSerial()
{
	return m_serial;
}

string PicoSCPIServer::GetFirmwareVersion()
{
	return m_fwver;
}

size_t PicoSCPIServer::GetAnalogChannelCount()
{
	return m_numChannels;
}

vector<size_t> PicoSCPIServer::GetSampleRates()
{
	vector<size_t> rates;
	vector<size_t> vec;
	double previousIntervalNs = 0;
	lock_guard<mutex> lock(m_mutex);
	//Enumerate timebases
	switch(m_pico_type)
	{
		case PICO2000A:
			if(m_model.find("2205MSO") != string::npos)
			{
				vec =
				{
					0,1,2,4,5,8,10,16,20,25,32,40,50,64,80,100,125,128,160,200,250,320,400,500,640,800,1000,1250,1280,1600,2000,2500,3200,4000,5000,6400,8000,10000,12500,12800,16000,20000,25000,32000,40000,50000,64000,80000,100000
				};
			}
			else if( m_model=="2206" || m_model=="2206A" || m_model=="2206B" || m_model=="2205AMSO" || m_model=="2405A" )
			{
				//!! 500 MS/s maximum sampling rate models
				vec =
				{
					0,1,2,3,4,6,7,10,12,22,27,42,52,82,102,127,202,252,402,502,627,802,1002,1252,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,20002,25002,31252,40002,50002,62502
				};
			}
			else
			{
				//!! 1 GS/s maximum sampling rate models
				vec =
				{
					0,1,2,3,4,6,7,10,12,18,22,27,42,52,82,102,127,162,202,252,402,502,627,802,1002,1252,1602,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,16002,20002,25002,31252,40002,50002,62502,80002,100002,125002
				};
			}
			break;
		case PICO3000A:
			if( (m_model[1]=='2') and (m_model[4]=='A' or m_model[4]=='B') )
			{
				//PicoScope 3000A and 3000B Series 2-Channel USB 2.0 Oscilloscopes
				vec =
				{
					0,1,2,3,4,6,7,10,12,22,27,42,52,82,102,127,202,252,402,502,627,802,1002,1252,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,20002,25002,31252,40002,50002,62502
				};
			}
			if( (m_model.find("MSO") != string::npos) and (m_model[4]!='D') )
			{
				//PicoScope 3000 Series USB 2.0 MSOs
				vec =
				{
					0,1,2,3,5,6,9,11,17,21,26,41,51,81,101,126,161,201,251,401,501,626,801,1001,1251,1601,2001,2501,3126,4001,5001,6251,8001,10001,12501,15626,16001,20001,25001,31251,40001,50001,62501,80001,100001,125001
				};
			}
			else
			{
				//PicoScope 3000A and 3000B Series 4-Channel USB 2.0 Oscilloscopes
				//PicoScope 3207A and 3207B USB 3.0 Oscilloscopes
				//PicoScope 3000D Series USB 3.0 Oscilloscopes and MSOs
				vec =
				{
					0,1,2,3,4,6,7,10,12,18,22,27,42,52,82,102,127,162,202,252,402,502,627,802,1002,1252,1602,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,16002,20002,25002,31252,40002,50002,62502,80002,100002,125002
				};
			}
			break;
		case PICO4000A:
			if(m_model.find("4444") != string::npos)
			{
				//PicoScope 4444
				vec =
				{
					0,1,2,3,4,6,7,10,12,18,22,27,34,42,52,66,82,102,127,162,202,252,322,402,502,627,642,802,1002,1252,1602,2002,2502,3202,4002,5002,6252,6402,8002,10002,12502,16002,20002,25002,32002,40002,50002
				};
			}
			else
			{
				//PicoScope 4824 and 4000A Series
				vec =
				{
					0,1,3,7,9,15,19,31,39,63,79,99,127,159,199,255,319,399,511,639,799,999,1023,1279,1599,1999,2559,3199,3999,5119,6399,7999,9999,10239,12799,15999,19999,25599,31999,39999,51199,63999,79999
				};
			}
			break;
		case PICO5000A:
			switch(m_adcBits)
			{
				case 8:
				{
					vec =
					{
						0,1,2,3,4,6,7,10,12,18,22,27,42,52,82,102,127,162,202,252,402,502,627,802,1002,1252,1602,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,16002,20002,25002,31252,40002,50002,62502,80002,100002,125002
					};
					break;
				}

				case 12:
				{
					vec =
					{
						1,2,3,4,5,7,8,11,13,23,28,43,53,83,103,128,203,253,403,503,628,803,1003,1253,2003,2503,3128,4003,5003,6253,8003,10003,12503,15628,20003,25003,31253,40003,50003,62503
					};
					break;
				}

				case 14:
				{
					vec =
					{
						3,4,6,7,10,12,18,22,27,42,52,82,102,127,162,202,252,402,502,627,802,1002,1252,1602,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,16002,20002,25002,31252,40002,50002,62502,80002,100002,125002
					};
					break;
				}

				case 15:
				{
					vec =
					{
						3,4,6,7,10,12,18,22,27,42,52,82,102,127,162,202,252,402,502,627,802,1002,1252,1602,2002,2502,3127,4002,5002,6252,8002,10002,12502,15627,16002,20002,25002,31252,40002,50002,62502,80002,100002,125002
					};
					break;
				}

				case 16:
				{
					vec =
					{
						4,5,7,8,11,13,23,28,43,53,83,103,128,203,253,403,503,628,803,1003,1253,2003,2503,3128,4003,5003,6253,8003,10003,12503,15628,20003,25003,31253,40003,50003,62503
					};
				}
			}
			break;
		case PICO6000A:
			//PicoScope 6428E-D
			if(m_model[3] == '8')
			{
				vec =
				{
					0,1,2,3,4,5,6,7,10,15,25,30,55,105,130,205,255,505,630,1005,1255,2005,2505,3130,5005,6255,10005,12505,15630,20005,25005,31255,50005,62505,78130,100005,125005,156255
				};
			}
			//PicoScope 6000E Series except the PicoScope 6428E-D
			else
			{
				vec =
				{
					0,1,2,3,4,5,6,9,14,24,29,54,104,129,204,254,504,629,1004,1254,2004,2504,3129,5004,6254,10004,12504,15629,20004,25004,31254,50004,62504,78129,100004,125004,156254
				};
			}
			break;
		case PICOPSOSPA:
			//All desired sample rates in picoseconds
			vec =
			{
				200,400,800,1000,1600,3200,6400,8000,10000,12500,12800,16000,20000,25000,32000,40000,50000,64000,80000,100000,125000,128000,160000,200000,250000,320000,400000,500000,640000,800000,1000000,1250000,1280000,1600000,2000000,2500000,3200000,4000000,5000000,6400000,8000000,10000000,12500000,12800000,16000000,20000000,25000000,32000000,40000000,50000000,64000000,80000000,100000000,125000000,128000000,160000000,200000000,250000000,320000000,400000000,500000000,640000000,800000000,1000000000
			};
			break;
	}

	for(auto i : vec)
	{
		double intervalNs;
		float intervalNs_f;
		uint64_t maxSamples;
		int32_t maxSamples_int;
		PICO_STATUS status = PICO_RESERVED_1;

		switch(m_pico_type)
		{
			case PICO2000A:
				status = m_api.ps2000aGetTimebase2(m_hScope, i, 1, &intervalNs_f, 1, &maxSamples_int, 0);
				maxSamples = maxSamples_int;
				intervalNs = intervalNs_f;
				break;
			case PICO3000A:
				status = m_api.ps3000aGetTimebase2(m_hScope, i, 1, &intervalNs_f, 1, &maxSamples_int, 0);
				maxSamples = maxSamples_int;
				intervalNs = intervalNs_f;
				break;
			case PICO4000A:
				status = m_api.ps4000aGetTimebase2(m_hScope, i, 1, &intervalNs_f, &maxSamples_int, 0);
				maxSamples = maxSamples_int;
				intervalNs = intervalNs_f;
				break;
			case PICO5000A:
				status = m_api.ps5000aGetTimebase2(m_hScope, i, 1, &intervalNs_f, &maxSamples_int, 0);
				maxSamples = maxSamples_int;
				intervalNs = intervalNs_f;
				break;
			case PICO6000A:
				status = m_api.ps6000aGetTimebase(m_hScope, i, 1, &intervalNs, &maxSamples, 0);
				break;
			case PICOPSOSPA:
				status = m_api.psospaGetTimebase(m_hScope, i, 1, &intervalNs, &maxSamples, 0);
				if(std::abs((intervalNs * 1000) - i) > 1)
					status = PICO_INVALID_TIMEBASE;		//Avoid irregular sample rates
				if(intervalNs == previousIntervalNs)
					status = PICO_INVALID_TIMEBASE;		//Avoid multiple entries of the same rate
				if(PICO_OK == status)
					previousIntervalNs = intervalNs;
				break;
		}

		if(PICO_OK == status)
		{
			size_t intervalFs = intervalNs * 1e6f;
			rates.push_back(FS_PER_SECOND / intervalFs);
			//LogDebug("GetTimebase:\t%ld\t%f\t%f\n", i, (1e12f / i), (FS_PER_SECOND / intervalFs));
		}
		else if( (PICO_INVALID_TIMEBASE == status) || (PICO_INVALID_CHANNEL == status) || (PICO_NO_CHANNELS_OR_PORTS_ENABLED == status) )
		{
			//Requested timebase not possible
			//This is common and harmless if we ask for e.g. timebase 0 when too many channels are active.
			continue;
		}
		else
			LogWarning("GetTimebase failed, code %d / 0x%x\n", status, status);
	}
	return rates;
}

vector<size_t> PicoSCPIServer::GetSampleDepths()
{
	vector<size_t> depths;

	lock_guard<mutex> lock(m_mutex);
	double intervalNs;
	float intervalNs_f;
	uint64_t maxSamples;
	int32_t maxSamples_int;

	PICO_STATUS status;
	status = PICO_RESERVED_1;

	//Ask for max memory depth at timebase number 10
	//We cannot use the first few timebases because those are sometimes not available depending on channel count etc
	int ntimebase = 10;
	switch(m_pico_type)
	{
		case PICO2000A:
			status = m_api.ps2000aGetTimebase2(m_hScope, ntimebase, 1, &intervalNs_f, 1, &maxSamples_int, 0);
			maxSamples = maxSamples_int;
			intervalNs = intervalNs_f;
			break;
		case PICO3000A:
			status = m_api.ps3000aGetTimebase2(m_hScope, ntimebase, 1, &intervalNs_f, 1, &maxSamples_int, 0);
			maxSamples = maxSamples_int;
			intervalNs = intervalNs_f;
			break;
		case PICO4000A:
			status = m_api.ps4000aGetTimebase2(m_hScope, ntimebase, 1, &intervalNs_f, &maxSamples_int, 0);
			maxSamples = maxSamples_int;
			intervalNs = intervalNs_f;
			break;
		case PICO5000A:
			status = m_api.ps5000aGetTimebase2(m_hScope, ntimebase, 1, &intervalNs_f, &maxSamples_int, 0);
			maxSamples = maxSamples_int;
			intervalNs = intervalNs_f;
			break;
		case PICO6000A:
			status = m_api.ps6000aGetTimebase(m_hScope, ntimebase, 1, &intervalNs, &maxSamples, 0);
			break;
		case PICOPSOSPA:
			status = m_api.psospaGetTimebase(m_hScope, 40000, 1, &intervalNs, &maxSamples, 0);
			break;
	}

	if(PICO_OK == status)
	{
		//Seems like there's no restrictions on actual memory depth other than an upper bound.
		//To keep things simple, report 1-2-5 series from 1K samples up to the actual max depth

		for(size_t base = 1000; base < maxSamples; base *= 10)
		{
			const size_t muls[] = {1, 2, 4, 5, 8};
			for(auto m : muls)
			{
				size_t depth = m * base;
				if(depth < maxSamples)
					depths.push_back(depth);
			}
		}

		depths.push_back(maxSamples);
	}

	return depths;
}

bool PicoSCPIServer::OnCommand(
	const string& line,
	const string& subject,
	const string& cmd,
	const vector<string>& args)
{
	//Function generator is different from normal channels
	//(uses range/offs commands so must go before normal bridge processing!)
	if(subject == "AWG")
	{
		if(cmd == "START")
		{
			lock_guard<mutex> lock(m_mutex);
			m_awgOn = true;
			ReconfigAWG();
		}

		else if(cmd == "STOP")
		{
			/*
			 * Special handling for Pico APIs except PS6000A and PSOSPA:
			 * Since they lack a dedicated stop command for signal generation,
			 * we achieve this by:
			 * 1. Temporarily setting AWG amplitude and offset to zero
			 * 2. Switching to software trigger mode
			 * 3. Restoring original AWG settings
			 *
			 * This ensures clean signal termination without residual voltage levels.
			 */
			lock_guard<mutex> lock(m_mutex);
			float tempRange = m_awgRange;
			float tempOffset = m_awgOffset;
			uint32_t status = PICO_OK;
			switch(m_pico_type)
			{
				case PICO2000A:
					tempRange = m_awgRange;
					tempOffset = m_awgOffset;
					m_awgRange = 0;
					m_awgOffset = 0;
					ReconfigAWG();
					status = m_api.ps2000aSetSigGenPropertiesBuiltIn(
									  m_hScope,
									  m_awgFreq,
									  m_awgFreq,
									  0,
									  0,
									  PS2000A_SWEEP_TYPE (0),
									  1,
									  0,
									  PS2000A_SIGGEN_RISING,
									  PS2000A_SIGGEN_SOFT_TRIG,
									  0
								  );
					if(status != PICO_OK)
						LogError("ps2000aSetSigGenPropertiesBuiltIn failed, code 0x%x \n", status);
					m_awgRange = tempRange;
					m_awgOffset = tempOffset;
					m_awgOn = false;
					break;
				case PICO3000A:
					tempRange = m_awgRange;
					tempOffset = m_awgOffset;
					m_awgRange = 0;
					m_awgOffset = 0;
					ReconfigAWG();
					status = m_api.ps3000aSetSigGenPropertiesBuiltIn(
									  m_hScope,
									  m_awgFreq,
									  m_awgFreq,
									  0,
									  0,
									  PS3000A_SWEEP_TYPE (0),
									  1,
									  0,
									  PS3000A_SIGGEN_RISING,
									  PS3000A_SIGGEN_SOFT_TRIG,
									  0
								  );
					if(status != PICO_OK)
						LogError("ps3000aSetSigGenPropertiesBuiltIn failed, code 0x%x \n", status);
					m_awgRange = tempRange;
					m_awgOffset = tempOffset;
					m_awgOn = false;
					break;
				case PICO4000A:
					tempRange = m_awgRange;
					tempOffset = m_awgOffset;
					m_awgRange = 0;
					m_awgOffset = 0;
					ReconfigAWG();
					status = m_api.ps4000aSetSigGenPropertiesBuiltIn(
									  m_hScope,
									  m_awgFreq,
									  m_awgFreq,
									  0,
									  0,
									  PS4000A_SWEEP_TYPE (0),
									  1,
									  0,
									  PS4000A_SIGGEN_RISING,
									  PS4000A_SIGGEN_SOFT_TRIG,
									  0
								  );
					if(status != PICO_OK)
						LogError("ps4000aSetSigGenPropertiesBuiltIn failed, code 0x%x \n", status);
					m_awgRange = tempRange;
					m_awgOffset = tempOffset;
					m_awgOn = false;
					break;
				case PICO5000A:
					tempRange = m_awgRange;
					tempOffset = m_awgOffset;
					m_awgRange = 0;
					m_awgOffset = 0;
					ReconfigAWG();
					status = m_api.ps5000aSetSigGenPropertiesBuiltIn(
									  m_hScope,
									  m_awgFreq,
									  m_awgFreq,
									  0,
									  0,
									  PS5000A_SWEEP_TYPE (0),
									  1,
									  0,
									  PS5000A_SIGGEN_RISING,
									  PS5000A_SIGGEN_SOFT_TRIG,
									  0
								  );
					if(status != PICO_OK)
						LogError("ps5000aSetSigGenPropertiesBuiltIn failed, code 0x%x \n", status);
					m_awgRange = tempRange;
					m_awgOffset = tempOffset;
					m_awgOn = false;
					break;
				case PICO6000A:
					m_awgOn = false;
					ReconfigAWG();
					break;
				case PICOPSOSPA:
					m_awgOn = false;
					ReconfigAWG();
					break;
			}

		}

		else if(args.size() == 1)
		{
			if(cmd == "FREQ")
			{
				lock_guard<mutex> lock(m_mutex);
				m_awgFreq = stof(args[0]);
				//Frequency must not be zero
				if(m_awgFreq<1e-3)
					m_awgFreq = 1;

				switch(m_pico_type)
				{
					case PICO2000A:
						//handled by ReconfigAWG()
						break;
					case PICO3000A:
						//handled by ReconfigAWG()
						break;
					case PICO4000A:
						//handled by ReconfigAWG()
						break;
					case PICO5000A:
						//handled by ReconfigAWG()
						break;
					case PICO6000A:
					{
						auto status = m_api.ps6000aSigGenFrequency(m_hScope, m_awgFreq);
						if(status != PICO_OK)
							LogError("ps6000aSigGenFrequency failed, code 0x%x (freq=%f)\n", status, m_awgFreq);
						break;
					}
					case PICOPSOSPA:
					{
						auto status = m_api.psospaSigGenFrequency(m_hScope, m_awgFreq);
						if(status != PICO_OK)
							LogError("psospaSigGenFrequency failed, code 0x%x (freq=%f)\n", status, m_awgFreq);
						break;
					}
				}
				ReconfigAWG();
			}

			else if(cmd == "DUTY")
			{
				lock_guard<mutex> lock(m_mutex);
				auto duty = stof(args[0]) * 100;
				uint32_t status = PICO_OK;

				switch(m_pico_type)
				{
					case PICO2000A:
						/* DutyCycle of square wave can not be controlled in ps2000a built in generator,
						Must be implemented via Arbitrary*/
						if( m_awgPS2000AWaveType == PS2000A_SQUARE )
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, (double) duty);
						else
							LogError("PICO2000A DUTY TODO code\n");
						break;
					case PICO3000A:
						/* DutyCycle of square wave can not be controlled in ps3000a built in generator,
						Must be implemented via Arbitrary*/
						if( m_awgPS3000AWaveType == PS3000A_SQUARE )
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, (double) duty);
						else
							LogError("PICO3000A DUTY TODO code\n");
						break;
					case PICO4000A:
						/* DutyCycle of square wave can not be controlled in ps4000a built in generator,
						Must be implemented via Arbitrary*/
						if( m_awgPS4000AWaveType == PS4000A_SQUARE )
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, (double) duty);
						else
							LogError("PICO4000A DUTY TODO code\n");
						break;
					case PICO5000A:
						/* DutyCycle of square wave can not be controlled in ps3000a built in generator,
						Must be implemented via Arbitrary*/
						if( m_awgPS5000AWaveType == PS5000A_SQUARE )
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, (double) duty);
						else
							LogError("PICO5000A DUTY TODO code\n");
						break;
					case PICO6000A:
						status = m_api.ps6000aSigGenWaveformDutyCycle(m_hScope, duty);
						if(status != PICO_OK)
							LogError("ps6000aSigGenWaveformDutyCycle failed, code 0x%x\n", status);
						//ReconfigAWG();
						break;
					case PICOPSOSPA:
						status = m_api.psospaSigGenWaveformDutyCycle(m_hScope, duty);
						if(status != PICO_OK)
							LogError("psospaSigGenWaveformDutyCycle failed, code 0x%x\n", status);
						break;
				}
				ReconfigAWG();
			}

			else if(cmd == "OFFS")
			{
				lock_guard<mutex> lock(m_mutex);
				m_awgOffset = stof(args[0]);

				ReconfigAWG();
			}

			else if(cmd == "RANGE")
			{
				lock_guard<mutex> lock(m_mutex);
				m_awgRange = stof(args[0]);

				ReconfigAWG();
			}

			else if(cmd == "SHAPE")
			{
				lock_guard<mutex> lock(m_mutex);

				auto waveform = g_waveformTypes.find(args[0]);
				if(waveform == g_waveformTypes.end())
				{
					LogError("Invalid waveform type: %s\n", args[0].c_str());
					return true;
				}

				uint32_t status = PICO_OK;
				switch(m_pico_type)
				{
					case PICO2000A:
						if( ( (args[0] == "WHITENOISE") || (args[0] == "PRBS") )
								&& ( (m_model == "2204A") || (m_model == "2205A") ) )
						{
							LogError("Noise/PRBS generation not supported by some 2xxxA Models\n");
							return true;
						}
						if( (m_awgPS2000AWaveType == PS2000A_SQUARE) )
						{
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, 50);
						}
						m_awgPS2000AWaveType = waveform->second.type2000;
						m_awgPS2000AOperation = waveform->second.op2000;
						if(args[0] == "ARBITRARY")
						{
							//TODO: find a more flexible way to specify arb buffer
							LogError("PICO2000A ARBITRARY TODO code\n");
						}
						break;
					case PICO3000A:
						if( ( (args[0] == "WHITENOISE") || (args[0] == "PRBS") )
								&& (m_model[4] == 'A' ) )
						{
							LogError("Noise/PRBS generation not supported by 3xxxA Models\n");
							return true;
						}
						if( (m_awgPS3000AWaveType == PS3000A_SQUARE) )
						{
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, 50);
						}
						m_awgPS3000AWaveType = waveform->second.type3000;
						m_awgPS3000AOperation = waveform->second.op3000;
						if(args[0] == "ARBITRARY")
						{
							//TODO: find a more flexible way to specify arb buffer
							LogError("PICO3000A ARBITRARY TODO code\n");
						}
						break;
					case PICO4000A:
						if( (m_awgPS4000AWaveType == PS4000A_SQUARE) )
						{
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, 50);
						}
						m_awgPS4000AWaveType = waveform->second.type4000;
						m_awgPS4000AOperation = waveform->second.op4000;
						if(args[0] == "ARBITRARY")
						{
							//TODO: find a more flexible way to specify arb buffer
							LogError("PICO4000A ARBITRARY TODO code\n");
						}
						break;
					case PICO5000A:
						if( (m_awgPS5000AWaveType == PS5000A_SQUARE) )
						{
							GenerateSquareWave(m_arbitraryWaveform, m_awgBufferSize, 50);
						}
						m_awgPS5000AWaveType = waveform->second.type5000;
						m_awgPS5000AOperation = waveform->second.op5000;


						if(args[0] == "ARBITRARY")
						{
							//TODO: find a more flexible way to specify arb buffer
							LogError("PICO5000A ARBITRARY TODO code\n");
						}
						break;
					case PICO6000A:
						status = m_api.ps6000aSigGenWaveform(m_hScope, waveform->second.type6000, NULL, 0);
						if(PICO_OK != status)
							LogError("ps6000aSigGenWaveform failed, code 0x%x\n", status);
						ReconfigAWG();
						if(args[0] == "ARBITRARY")
						{
							//TODO: ReconfigAWG() can handle this already, must only fill the buffer
							LogError("PICO6000A ARBITRARY TODO code\n");
						}
						break;
					case PICOPSOSPA:
						status = m_api.psospaSigGenWaveform(m_hScope, waveform->second.type6000, NULL, 0);
						if(PICO_OK != status)
							LogError("psospaSigGenWaveform failed, code 0x%x\n", status);
						ReconfigAWG();
						if(args[0] == "ARBITRARY")
						{
							//TODO: ReconfigAWG() can handle this already, must only fill the buffer
							LogError("PICOPSOSPA ARBITRARY TODO code\n");
						}
						break;
				}

				ReconfigAWG();
			}
			else
				LogError("Unrecognized AWG command %s\n", line.c_str());
		}
		else
			LogError("Unrecognized AWG command %s\n", line.c_str());
	}
	else if(BridgeSCPIServer::OnCommand(line, subject, cmd, args))
		return true;

	else if( (cmd == "BITS") && (args.size() == 1) )
	{
		lock_guard<mutex> lock(m_mutex);
		int bits = stoi(args[0]);
		switch(m_pico_type)
		{
			case PICO2000A:
				m_adcBits = 8;
				return false;
				break;
			case PICO3000A:
				m_adcBits = 8;
				return false;
				break;
			case PICO4000A:
				if(m_model.find("4444") != string::npos)
				{
					m_api.ps4000aStop(m_hScope);

					//Changing the ADC resolution necessitates reallocation of the buffers
					//due to different memory usage.
					m_memDepthChanged = true;

					switch(bits)
					{
						case 12:
							m_adcBits = bits;
							m_api.ps4000aSetDeviceResolution(m_hScope, PS4000A_DR_12BIT);
							break;

						case 14:
							m_adcBits = bits;
							m_api.ps4000aSetDeviceResolution(m_hScope, PS4000A_DR_14BIT);
							break;

						default:
							LogError("User requested invalid resolution (%d bits)\n", bits);
					}

					if(m_triggerArmed)
						StartCapture(false);
					//update all active channels
					for(size_t i=0; i<m_numChannels; i++)
					{
						if(m_channelOn[i])
							UpdateChannel(i);
					}
				}
				else
				{
					m_adcBits = 12;
					return false;
				}
				break;
			case PICO5000A:
				m_api.ps5000aStop(m_hScope);

				//Changing the ADC resolution necessitates reallocation of the buffers
				//due to different memory usage.
				m_memDepthChanged = true;

				switch(bits)
				{
					case 8:
						m_adcBits = bits;
						m_api.ps5000aSetDeviceResolution(m_hScope, PS5000A_DR_8BIT);
						break;

					case 12:
						m_adcBits = bits;
						m_api.ps5000aSetDeviceResolution(m_hScope, PS5000A_DR_12BIT);
						break;

					case 14:
						m_adcBits = bits;
						m_api.ps5000aSetDeviceResolution(m_hScope, PS5000A_DR_14BIT);
						break;

					case 15:
						m_adcBits = bits;
						m_api.ps5000aSetDeviceResolution(m_hScope, PS5000A_DR_15BIT);
						break;

					case 16:
						m_adcBits = bits;
						m_api.ps5000aSetDeviceResolution(m_hScope, PS5000A_DR_16BIT);
						break;

					default:
						LogError("User requested invalid resolution (%d bits)\n", bits);
				}

				if(m_triggerArmed)
					StartCapture(false);
				//update all active channels
				for(size_t i=0; i<m_numChannels; i++)
				{
					if(m_channelOn[i])
						UpdateChannel(i);
				}
				break;
			case PICO6000A:
				m_api.ps6000aStop(m_hScope);

				//Even though we didn't actually change memory, apparently calling m_api.ps6000aSetDeviceResolution
				//will invalidate the existing buffers and make m_api.ps6000aGetValues() fail with PICO_BUFFERS_NOT_SET.
				m_memDepthChanged = true;

				switch(bits)
				{
					case 8:
						m_adcBits = bits;
						m_api.ps6000aSetDeviceResolution(m_hScope, PICO_DR_8BIT);
						break;

					case 10:
						m_adcBits = bits;
						m_api.ps6000aSetDeviceResolution(m_hScope, PICO_DR_10BIT);
						break;

					case 12:
						m_adcBits = bits;
						m_api.ps6000aSetDeviceResolution(m_hScope, PICO_DR_12BIT);
						break;

					default:
						LogError("User requested invalid resolution (%d bits)\n", bits);
				}

				if(m_triggerArmed)
					StartCapture(false);
				//update all active channels
				for(size_t i=0; i<m_numChannels; i++)
				{
					if(m_channelOn[i])
						UpdateChannel(i);
				}
				break;
			case PICOPSOSPA:
				m_api.psospaStop(m_hScope);
				m_memDepthChanged = true;

				switch(bits)
				{
					case 8:
						m_adcBits = bits;
						m_api.psospaSetDeviceResolution(m_hScope, PICO_DR_8BIT);
						break;

					case 10:
						m_adcBits = bits;
						m_api.psospaSetDeviceResolution(m_hScope, PICO_DR_10BIT);
						break;

					default:
						LogError("User requested invalid resolution (%d bits)\n", bits);
				}

				if(m_triggerArmed)
					StartCapture(false);
				//update all active channels
				for(size_t i=0; i<m_numChannels; i++)
				{
					if(m_channelOn[i])
						UpdateChannel(i);
				}
				break;
		}
	}

	else if( (cmd == "BWLIM") && (args.size() == 1) )
	{
		//Extract channel ID from subject and clamp bounds
		size_t channelId = 0;
		if(isalpha(subject[0]))
		{
			channelId = min(static_cast<size_t>(subject[0] - 'A'), m_numChannels);
			//channelIsDigital = false;
		}
		lock_guard<mutex> lock(m_mutex);

		int freq_mhz = stoi(args[0]);
		SetChannelBandwidthLimiter(channelId, freq_mhz);
	}

	else if( (cmd == "RANGE") )
	{
		//This will be called when digital channels are on the same View with analog channels and voltage range is changed.
		//Just do nothing here to avoid spamming the debug log with "Unrecognized command received".
		return false;
	}

	else
	{
		LogDebug("Unrecognized command received: %s\n", line.c_str());
		LogIndenter li;
		LogDebug("Subject: %s\n", subject.c_str());
		LogDebug("Command: %s\n", cmd.c_str());
		for(auto arg : args)
			LogDebug("Arg: %s\n", arg.c_str());
		return false;
	}

	return true;
}

void PicoSCPIServer::SetChannelBandwidthLimiter(size_t chan, unsigned int limit_mhz)
{

	switch(m_pico_type)
	{
		case PICO2000A:
			//no limiters on this series
			break;
		case PICO3000A:
			if(limit_mhz == 20)
				m_bandwidth_3000a[chan] = PS3000A_BW_20MHZ;
			else
				m_bandwidth_3000a[chan] = PS3000A_BW_FULL;
			break;
		case PICO4000A:
			if(limit_mhz == 1)
				m_bandwidth_4000a[chan] = PS4000A_BW_1MHZ;
			else if(limit_mhz == 100)	//workaround: use 100MHz for 100kHz filter (applicable to 4444 (20MHz bandwidth))
				m_bandwidth_4000a[chan] = PS4000A_BW_100KHZ;
			else
				m_bandwidth_4000a[chan] = PS4000A_BW_FULL;
			break;
		case PICO5000A:
			if(limit_mhz == 20)
				m_bandwidth_5000a[chan] = PS5000A_BW_20MHZ;
			else
				m_bandwidth_5000a[chan] = PS5000A_BW_FULL;
			break;
		case PICO6000A:
			if(limit_mhz == 20)
				m_bandwidth[chan] = PICO_BW_20MHZ;
			else if(limit_mhz == 200)
				m_bandwidth[chan] = PICO_BW_200MHZ;
			else
				m_bandwidth[chan] = PICO_BW_FULL;
			break;
		case PICOPSOSPA:
			if(limit_mhz == 20)
				m_bandwidth[chan] = PICO_BW_20MHZ;
			else if(limit_mhz == 50)
				m_bandwidth[chan] = PICO_BW_50MHZ;
			else if(limit_mhz == 100)
				m_bandwidth[chan] = PICO_BW_100MHZ;
			else if(limit_mhz == 200)
				m_bandwidth[chan] = PICO_BW_200MHZ;
			else if(limit_mhz == 350)
				m_bandwidth[chan] = PICO_BW_350MHZ;
			else if(limit_mhz == 500)
				m_bandwidth[chan] = PICO_BW_500MHZ;
			else
				m_bandwidth[chan] = PICO_BW_FULL;
			break;
	}

	UpdateChannel(chan);
}

/**
	@brief Reconfigures the function generator
 */
void PicoSCPIServer::ReconfigAWG()
{
	double freq = m_awgFreq;
	double inc = 0;
	double dwell = 0;
	float tempRange = m_awgRange;
	float tempOffset = m_awgOffset;
	uint32_t status = PICO_OK;
	if(!m_awgOn)
	{
		tempRange = 0;
		tempOffset = 0;
	}

	switch(m_pico_type)
	{
		case PICO2000A:
			Stop(); // Need to stop acquisition when setting the AWG to avoid "PICO_BUSY" errors
			if(m_awgPS2000AWaveType == PS2000A_SQUARE || m_awgPS2000AWaveType == PS2000A_MAX_WAVE_TYPES)
			{
				uint32_t delta= 0;
				status = m_api.ps2000aSigGenFrequencyToPhase(m_hScope, m_awgFreq, PS2000A_SINGLE, m_awgBufferSize, &delta);
				if(status != PICO_OK)
					LogError("ps2000aSigGenFrequencyToPhase failed, code 0x%x\n", status);
				status =  m_api.ps2000aSetSigGenArbitrary(
							  m_hScope,
							  tempOffset*1e6,
							  tempRange*1e6*2,
							  delta,
							  delta,
							  0,
							  0,
							  m_arbitraryWaveform,
							  m_awgBufferSize,
							  PS2000A_UP,          // sweepType
							  PS2000A_ES_OFF,      // operation
							  PS2000A_SINGLE,      // indexMode
							  PS2000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,
							  0,
							  PS2000A_SIGGEN_RISING,
							  PS2000A_SIGGEN_NONE,
							  0);
				if(status != PICO_OK)
					LogError("ps2000aSetSigGenArbitrary failed, code 0x%x\n", status);
			}
			else
			{
				status = m_api.ps2000aSetSigGenBuiltInV2(
								  m_hScope,
								  tempOffset*1e6,        //Offset Voltage in µV
								  tempRange *1e6*2,      // Peak to Peak Range in µV
								  m_awgPS2000AWaveType,
								  freq,
								  freq,
								  inc,
								  dwell,
								  PS2000A_UP,
								  m_awgPS2000AOperation,
								  PS2000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,  //run forever
								  0,  //dont use sweeps
								  PS2000A_SIGGEN_RISING,
								  PS2000A_SIGGEN_NONE,
								  0);                         // Tigger level (-32767 to 32767 -> -5 to 5 V)
				if(PICO_OK != status)
					LogError("ps2000aSetSigGenBuiltInV2 failed, code 0x%x\n", status);
			}
			if(m_triggerArmed)
				StartCapture(false);
			break;
		case PICO3000A:
			Stop(); // Need to stop acquisition when setting the AWG to avoid "PICO_BUSY" errors
			if(m_awgPS3000AWaveType == PS3000A_SQUARE || m_awgPS3000AWaveType == PS3000A_MAX_WAVE_TYPES)
			{
				uint32_t delta= 0;
				status = m_api.ps3000aSigGenFrequencyToPhase(m_hScope, m_awgFreq, PS3000A_SINGLE, m_awgBufferSize, &delta);
				if(status != PICO_OK)
					LogError("ps3000aSigGenFrequencyToPhase failed, code 0x%x\n", status);
				status =  m_api.ps3000aSetSigGenArbitrary(
							  m_hScope,
							  tempOffset*1e6,
							  tempRange*1e6*2,
							  delta,
							  delta,
							  0,
							  0,
							  m_arbitraryWaveform,
							  m_awgBufferSize,
							  PS3000A_UP,          // sweepType
							  PS3000A_ES_OFF,      // operation
							  PS3000A_SINGLE,      // indexMode
							  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,
							  0,
							  PS3000A_SIGGEN_RISING,
							  PS3000A_SIGGEN_NONE,
							  0);
				if(status != PICO_OK)
					LogError("ps3000aSetSigGenArbitrary failed, code 0x%x\n", status);
			}
			else
			{
				status = m_api.ps3000aSetSigGenBuiltInV2(
								  m_hScope,
								  tempOffset*1e6,        //Offset Voltage in µV
								  tempRange *1e6*2,      // Peak to Peak Range in µV
								  m_awgPS3000AWaveType,
								  freq,
								  freq,
								  inc,
								  dwell,
								  PS3000A_UP,
								  m_awgPS3000AOperation,
								  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,  //run forever
								  0,  //dont use sweeps
								  PS3000A_SIGGEN_RISING,
								  PS3000A_SIGGEN_NONE,
								  0);                         // Tigger level (-32767 to 32767 -> -5 to 5 V)
				if(PICO_OK != status)
					LogError("ps3000aSetSigGenBuiltInV2 failed, code 0x%x\n", status);
			}
			if(m_triggerArmed)
				StartCapture(false);
			break;
		case PICO4000A:
			Stop(); // Need to stop acquisition when setting the AWG to avoid "PICO_BUSY" errors
			if(m_awgPS4000AWaveType == PS4000A_SQUARE || m_awgPS4000AWaveType == PS4000A_MAX_WAVE_TYPES)
			{
				uint32_t delta= 0;
				status = m_api.ps4000aSigGenFrequencyToPhase(m_hScope, m_awgFreq, PS4000A_SINGLE, m_awgBufferSize, &delta);
				if(status != PICO_OK)
					LogError("ps3000aSigGenFrequencyToPhase failed, code 0x%x\n", status);
				status =  m_api.ps4000aSetSigGenArbitrary(
							  m_hScope,
							  tempOffset*1e6,
							  tempRange*1e6*2,
							  delta,
							  delta,
							  0,
							  0,
							  m_arbitraryWaveform,
							  m_awgBufferSize,
							  PS4000A_UP,          // sweepType
							  PS4000A_ES_OFF,      // operation
							  PS4000A_SINGLE,      // indexMode
							  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,
							  0,
							  PS4000A_SIGGEN_RISING,
							  PS4000A_SIGGEN_NONE,
							  0);
				if(status != PICO_OK)
					LogError("ps4000aSetSigGenArbitrary failed, code 0x%x\n", status);
			}
			else
			{
				status = m_api.ps4000aSetSigGenBuiltInV2(
								  m_hScope,
								  tempOffset*1e6,        //Offset Voltage in µV
								  tempRange *1e6*2,      // Peak to Peak Range in µV
								  m_awgPS4000AWaveType,
								  freq,
								  freq,
								  inc,
								  dwell,
								  PS4000A_UP,
								  m_awgPS4000AOperation,
								  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,  //run forever
								  0,  //dont use sweeps
								  PS4000A_SIGGEN_RISING,
								  PS4000A_SIGGEN_NONE,
								  0);                         // Tigger level (-32767 to 32767 -> -5 to 5 V)
				if(PICO_OK != status)
					LogError("ps4000aSetSigGenBuiltInV2 failed, code 0x%x\n", status);
			}
			if(m_triggerArmed)
				StartCapture(false);
			break;
		case PICO5000A:
			Stop(); // Need to stop acquisition when setting the AWG to avoid "PICO_BUSY" errors
			if(m_awgPS5000AWaveType == PS5000A_SQUARE || m_awgPS5000AWaveType == PS5000A_MAX_WAVE_TYPES)
			{
				uint32_t delta= 0;
				status = m_api.ps5000aSigGenFrequencyToPhase(m_hScope, m_awgFreq, PS5000A_SINGLE, m_awgBufferSize, &delta);
				if(status != PICO_OK)
					LogError("ps5000aSigGenFrequencyToPhase failed, code 0x%x\n", status);
				status =  m_api.ps5000aSetSigGenArbitrary(
							  m_hScope,
							  tempOffset*1e6,
							  tempRange*1e6*2,
							  delta,
							  delta,
							  0,
							  0,
							  m_arbitraryWaveform,
							  m_awgBufferSize,
							  PS5000A_UP,          // sweepType
							  PS5000A_ES_OFF,      // operation
							  PS5000A_SINGLE,      // indexMode
							  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,
							  0,
							  PS5000A_SIGGEN_RISING,
							  PS5000A_SIGGEN_NONE,
							  0);
				if(status != PICO_OK)
					LogError("ps5000aSetSigGenArbitrary failed, code 0x%x\n", status);
			}
			else
			{
				status = m_api.ps5000aSetSigGenBuiltInV2(
								  m_hScope,
								  tempOffset*1e6,        //Offset Voltage in µV
								  tempRange*1e6*2,       // Peak to Peak Range in µV
								  m_awgPS5000AWaveType,
								  freq,
								  freq,
								  inc,
								  dwell,
								  PS5000A_UP,
								  m_awgPS5000AOperation,
								  PS3000A_SHOT_SWEEP_TRIGGER_CONTINUOUS_RUN,  //run forever
								  0,  //dont use sweeps
								  PS5000A_SIGGEN_RISING,
								  PS5000A_SIGGEN_NONE,
								  0);                         // Tigger level (-32767 to 32767 -> -5 to 5 V)
				if(PICO_OK != status)
					LogError("ps5000aSetSigGenBuiltInV2 failed, code 0x%x\n", status);
			}
			if(m_triggerArmed)
				StartCapture(false);
			break;
		case PICO6000A:
			status = m_api.ps6000aSigGenRange(m_hScope, m_awgRange, m_awgOffset);
			if(PICO_OK != status)
				LogError("ps6000aSigGenRange failed, code 0x%x\n", status);

			status = m_api.ps6000aSigGenApply(
						 m_hScope,
						 m_awgOn,
						 false,		//sweep enable
						 false,		//trigger enable
						 true,		//automatic DDS sample frequency
						 false,		//do not override clock and prescale
						 &freq,
						 &freq,
						 &inc,
						 &dwell);
			if(PICO_OK != status)
				LogError("ps6000aSigGenApply failed, code 0x%x\n", status);
			break;
		case PICOPSOSPA:
			status = m_api.psospaSigGenRange(m_hScope, m_awgRange, m_awgOffset);
			if(PICO_OK != status)
				LogError("psospaSigGenRange failed, code 0x%x\n", status);

			status = m_api.psospaSigGenApply(
						 m_hScope,
						 m_awgOn,
						 0,		//sweep enable
						 0,		//trigger enable
						 &freq,
						 &freq,
						 &inc,
						 &dwell);
			if(PICO_OK != status)
			{
				LogError("psospaSigGenApply failed, code 0x%x\n", status);
				LogError("psospaSigGenApply failed, freq %f\n", freq);
			}
			break;
	}
}

bool PicoSCPIServer::GetChannelID(const std::string& subject, size_t& id_out)
{
	if(subject == "EX")
	{
		id_out = PICO_TRIGGER_AUX;
		return true;
	}

	//Extract channel ID from subject and clamp bounds
	size_t channelId = 0;
	size_t laneId = 0;
	bool channelIsDigital = false;
	if(isalpha(subject[0]))
	{
		channelId = min(static_cast<size_t>(subject[0] - 'A'), m_numChannels);
		channelIsDigital = false;
	}
	else if(isdigit(subject[0]))
	{
		channelId = min(subject[0] - '0', 2) - 1;
		channelIsDigital = true;
		if(subject.length() >= 3)
			laneId = min(subject[2] - '0', 7);
	}
	else
		return false;

	//Pack channel IDs into bytes
	//Byte 0: channel / pod ID
	//Byte 1: lane ID
	//Byte 2: digital flag
	id_out = channelId;
	if(channelIsDigital)
		id_out |= 0x800000 | (laneId << 8);

	return true;
}

BridgeSCPIServer::ChannelType PicoSCPIServer::GetChannelType(size_t channel)
{
	if(channel == PICO_TRIGGER_AUX)
		return CH_EXTERNAL_TRIGGER;
	else if(channel > 0xff)
		return CH_DIGITAL;
	else
		return CH_ANALOG;
}

void PicoSCPIServer::AcquisitionStart(bool oneShot)
{
	lock_guard<mutex> lock(m_mutex);

	if(m_triggerArmed)
	{
		LogVerbose("Ignoring START command because trigger is already armed\n");
		return;
	}

	//Make sure we've got something to capture
	bool anyChannels = false;
	for(size_t i=0; i<m_numChannels; i++)
	{
		if(m_channelOn[i])
		{
			anyChannels = true;
			break;
		}
	}
	for(size_t i=0; i<m_numDigitalPods; i++)
	{
		if(m_msoPodEnabled[i])
		{
			anyChannels = true;
			break;
		}
	}

	if(!anyChannels)
	{
		LogVerbose("Ignoring START command because no channels are active\n");
		return;
	}

	//Start the capture
	StartCapture(false);
	m_triggerOneShot = oneShot;
}

void PicoSCPIServer::AcquisitionForceTrigger()
{
	//Clear out any old trigger config
	if(m_triggerArmed)
	{
		Stop();
		m_triggerArmed = false;
	}

	lock_guard<mutex> lock(m_mutex);

	UpdateTrigger(true);
	StartCapture(true, true);
}

void PicoSCPIServer::AcquisitionStop()
{
	Stop();

	lock_guard<mutex> lock(m_mutex);

	//Convert any in-progress trigger to one shot.
	//This ensures that if a waveform is halfway through being downloaded, we won't re-arm the trigger after it finishes.
	m_triggerOneShot = true;
	m_triggerArmed = false;
}

void PicoSCPIServer::SetChannelEnabled(size_t chIndex, bool enabled)
{
	lock_guard<mutex> lock(m_mutex);
	uint32_t status = PICO_OK;

	if(GetChannelType(chIndex) == CH_DIGITAL)
	{
		int podIndex = (chIndex & 0xff);
		PICO_CHANNEL podId = (PICO_CHANNEL)(PICO_PORT0 + podIndex);

		if(enabled)
		{
			switch(m_pico_type)
			{
				case PICO2000A:
					status = m_api.ps2000aSetDigitalPort(m_hScope, (PS2000A_DIGITAL_PORT)podId, 1, m_msoPodThreshold[podIndex][0]);
					if(status != PICO_OK)
						LogError("ps2000aSetDigitalPort to on failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = true;
					break;
				case PICO3000A:
					status = m_api.ps3000aSetDigitalPort(m_hScope, (PS3000A_DIGITAL_PORT)podId, 1, m_msoPodThreshold[podIndex][0]);
					if(status != PICO_OK)
						LogError("ps3000aSetDigitalPort to on failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = true;
					break;
				case PICO4000A:
					//no digital channels on this series
					break;
				case PICO5000A:
					status = m_api.ps5000aSetDigitalPort(m_hScope, (PS5000A_CHANNEL)podId, 1, m_msoPodThreshold[podIndex][0]);
					if(status != PICO_OK)
						LogError("ps5000aSetDigitalPort to on failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = true;
					break;
				case PICO6000A:
					status = m_api.ps6000aSetDigitalPortOn(
									  m_hScope,
									  podId,
									  m_msoPodThreshold[podIndex],
									  8,
									  m_msoHysteresis[podIndex]);
					if(status != PICO_OK)
						LogError("ps6000aSetDigitalPortOn failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = true;
					break;
				case PICOPSOSPA:
					status = m_api.psospaSetDigitalPortOn(
									  m_hScope,
									  podId,
									  m_msoPodThreshold[podIndex][0]);
					if(status != PICO_OK)
						LogError("psospaSetDigitalPortOn failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = true;
					break;
			}
		}
		else
		{
			switch(m_pico_type)
			{
				case PICO2000A:
					status = m_api.ps2000aSetDigitalPort(m_hScope, (PS2000A_DIGITAL_PORT)podId, 0, 0);
					if(status != PICO_OK)
						LogError("ps2000aSetDigitalPort to off failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = false;
					break;
				case PICO3000A:
					status = m_api.ps3000aSetDigitalPort(m_hScope, (PS3000A_DIGITAL_PORT)podId, 0, 0);
					if(status != PICO_OK)
						LogError("ps3000aSetDigitalPort to off failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = false;
					break;
				case PICO4000A:
					//no digital channels on this series
					break;
				case PICO5000A:
					status = m_api.ps5000aSetDigitalPort(m_hScope, (PS5000A_CHANNEL)podId, 0, 0);
					if(status != PICO_OK)
						LogError("ps5000aSetDigitalPort to off failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = false;
					break;
				case PICO6000A:
					status = m_api.ps6000aSetDigitalPortOff(m_hScope, podId);
					if(status != PICO_OK)
						LogError("ps6000aSetDigitalPortOff failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = false;
					break;
				case PICOPSOSPA:
					status = m_api.psospaSetDigitalPortOff(m_hScope, podId);
					if(status != PICO_OK)
						LogError("psospaSetDigitalPortOff failed with code %x\n", status);
					else
						m_msoPodEnabled[podIndex] = false;
					break;
			}
		}
	}
	else
	{
		int chId = chIndex & 0xff;
		m_channelOn[chId] = enabled;
		UpdateChannel(chId);
	}

	//We need to allocate new buffers for this channel
	m_memDepthChanged = true;
	UpdateTrigger(); //TESTING lasse
}

void PicoSCPIServer::SetAnalogCoupling(size_t chIndex, const std::string& coupling)
{
	lock_guard<mutex> lock(m_mutex);
	int channelId = chIndex & 0xff;

	if(coupling == "DC1M")
		m_coupling[channelId] = PICO_DC;
	else if(coupling == "AC1M")
		m_coupling[channelId] = PICO_AC;
	else if(coupling == "DC50")
		m_coupling[channelId] = PICO_DC_50OHM;

	UpdateChannel(channelId);
}

void PicoSCPIServer::SetAnalogRange(size_t chIndex, double range_V)
{
	lock_guard<mutex> lock(m_mutex);

	size_t channelId = chIndex & 0xff;
	//range_V is peak-to-peak whereas the Pico modes are V-peak,
	//i.e. PS5000_20V = +-20V = 40Vpp = 'range_V = 40'
	range_V = range_V / 2;

	switch(m_pico_type)
	{
		case PICO2000A:
			//2000 series uses passive probes only, 20mV to 20V, no 50 ohm mode available
			if(range_V > 10)
			{
				m_range_2000a[channelId] = PS2000A_20V;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range_2000a[channelId] = PS2000A_10V;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range_2000a[channelId] = PS2000A_5V;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range_2000a[channelId] = PS2000A_2V;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range_2000a[channelId] = PS2000A_1V;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range_2000a[channelId] = PS2000A_500MV;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range_2000a[channelId] = PS2000A_200MV;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range_2000a[channelId] = PS2000A_100MV;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range_2000a[channelId] = PS2000A_50MV;
				m_roundedRange[channelId] = 0.05;
			}
			else
			{
				m_range_2000a[channelId] = PS2000A_20MV;
				m_roundedRange[channelId] = 0.02;
			}
			break;
		case PICO3000A:
			//3000D series uses passive probes only, 20mV to 20V, no 50 ohm mode available
			if(range_V > 10)
			{
				m_range_3000a[channelId] = PS3000A_20V;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range_3000a[channelId] = PS3000A_10V;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range_3000a[channelId] = PS3000A_5V;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range_3000a[channelId] = PS3000A_2V;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range_3000a[channelId] = PS3000A_1V;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range_3000a[channelId] = PS3000A_500MV;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range_3000a[channelId] = PS3000A_200MV;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range_3000a[channelId] = PS3000A_100MV;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range_3000a[channelId] = PS3000A_50MV;
				m_roundedRange[channelId] = 0.05;
			}
			else
			{
				m_range_3000a[channelId] = PS3000A_20MV;
				m_roundedRange[channelId] = 0.02;
			}
			break;
		case PICO4000A:
			//4000 series uses passive probes only, 10mV to 50V, no 50 ohm mode available
			if(range_V > 20)
			{
				m_range_4000a[channelId] = PS4000A_50V;
				m_range[channelId] = PICO_X1_PROBE_50V;
				m_roundedRange[channelId] = 50;
			}
			else if(range_V > 10)
			{
				m_range_4000a[channelId] = PS4000A_20V;
				m_range[channelId] = PICO_X1_PROBE_20V;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range_4000a[channelId] = PS4000A_10V;
				m_range[channelId] = PICO_X1_PROBE_10V;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range_4000a[channelId] = PS4000A_5V;
				m_range[channelId] = PICO_X1_PROBE_5V;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range_4000a[channelId] = PS4000A_2V;
				m_range[channelId] = PICO_X1_PROBE_2V;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range_4000a[channelId] = PS4000A_1V;
				m_range[channelId] = PICO_X1_PROBE_1V;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range_4000a[channelId] = PS4000A_500MV;
				m_range[channelId] = PICO_X1_PROBE_500MV;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range_4000a[channelId] = PS4000A_200MV;
				m_range[channelId] = PICO_X1_PROBE_200MV;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range_4000a[channelId] = PS4000A_100MV;
				m_range[channelId] = PICO_X1_PROBE_100MV;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range_4000a[channelId] = PS4000A_50MV;
				m_range[channelId] = PICO_X1_PROBE_50MV;
				m_roundedRange[channelId] = 0.05;
			}
			else if(range_V > 0.01)
			{
				m_range_4000a[channelId] = PS4000A_20MV;
				m_range[channelId] = PICO_X1_PROBE_20MV;
				m_roundedRange[channelId] = 0.02;
			}
			else
			{
				m_range_4000a[channelId] = PS4000A_10MV;
				m_range[channelId] = PICO_X1_PROBE_10MV;
				m_roundedRange[channelId] = 0.01;
			}
			break;
		case PICO5000A:
			//5000D series uses passive probes only, 10mV to 20V, no 50 ohm mode available
			if(range_V > 10)
			{
				m_range_5000a[channelId] = PS5000A_20V;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range_5000a[channelId] = PS5000A_10V;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range_5000a[channelId] = PS5000A_5V;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range_5000a[channelId] = PS5000A_2V;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range_5000a[channelId] = PS5000A_1V;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range_5000a[channelId] = PS5000A_500MV;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range_5000a[channelId] = PS5000A_200MV;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range_5000a[channelId] = PS5000A_100MV;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range_5000a[channelId] = PS5000A_50MV;
				m_roundedRange[channelId] = 0.05;
			}
			else if(range_V > 0.01)
			{
				m_range_5000a[channelId] = PS5000A_20MV;
				m_roundedRange[channelId] = 0.02;
			}
			else
			{
				m_range_5000a[channelId] = PS5000A_10MV;
				m_roundedRange[channelId] = 0.01;
			}
			break;
		case PICO6000A:
			//6000E series can use intelligent probes.
			//Model 6428E-D is 50 ohm only and has a limited range.
			//If 50 ohm coupling, cap hardware voltage range to 5V
			if(m_coupling[channelId] == PICO_DC_50OHM)
				range_V = min(range_V, 5.0);

			if(range_V > 100)
			{
				m_range[channelId] = PICO_X1_PROBE_200V;
				m_roundedRange[channelId] = 200;
			}
			else if(range_V > 50)
			{
				m_range[channelId] = PICO_X1_PROBE_100V;
				m_roundedRange[channelId] = 100;
			}
			else if(range_V > 20)
			{
				m_range[channelId] = PICO_X1_PROBE_50V;
				m_roundedRange[channelId] = 50;
			}
			else if(range_V > 10)
			{
				m_range[channelId] = PICO_X1_PROBE_20V;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range[channelId] = PICO_X1_PROBE_10V;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range[channelId] = PICO_X1_PROBE_5V;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range[channelId] = PICO_X1_PROBE_2V;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range[channelId] = PICO_X1_PROBE_1V;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range[channelId] = PICO_X1_PROBE_500MV;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range[channelId] = PICO_X1_PROBE_200MV;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range[channelId] = PICO_X1_PROBE_100MV;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range[channelId] = PICO_X1_PROBE_50MV;
				m_roundedRange[channelId] = 0.05;
			}
			else if(range_V > 0.01)
			{
				m_range[channelId] = PICO_X1_PROBE_20MV;
				m_roundedRange[channelId] = 0.02;
			}
			else
			{
				m_range[channelId] = PICO_X1_PROBE_10MV;
				m_roundedRange[channelId] = 0.01;
			}
			break;
		case PICOPSOSPA:
			//3000E series uses passive probes only, 5mV to 20V, 50 ohm mode is available.
			//range is set in nanovolts!
			//If 50 ohm coupling, cap hardware voltage range to 5V
			if(m_coupling[channelId] == PICO_DC_50OHM)
				range_V = min(range_V, 5.0);

			if(range_V > 10)
			{
				m_range_3000e[channelId] = 20e9;
				m_roundedRange[channelId] = 20;
			}
			else if(range_V > 5)
			{
				m_range_3000e[channelId] = 10e9;
				m_roundedRange[channelId] = 10;
			}
			else if(range_V > 2)
			{
				m_range_3000e[channelId] = 5e9;
				m_roundedRange[channelId] = 5;
			}
			else if(range_V > 1)
			{
				m_range_3000e[channelId] = 2e9;
				m_roundedRange[channelId] = 2;
			}
			else if(range_V > 0.5)
			{
				m_range_3000e[channelId] = 1e9;
				m_roundedRange[channelId] = 1;
			}
			else if(range_V > 0.2)
			{
				m_range_3000e[channelId] = 5e8;
				m_roundedRange[channelId] = 0.5;
			}
			else if(range_V > 0.1)
			{
				m_range_3000e[channelId] = 2e8;
				m_roundedRange[channelId] = 0.2;
			}
			else if(range_V > 0.05)
			{
				m_range_3000e[channelId] = 1e8;
				m_roundedRange[channelId] = 0.1;
			}
			else if(range_V > 0.02)
			{
				m_range_3000e[channelId] = 5e7;
				m_roundedRange[channelId] = 0.05;
			}
			else if(range_V > 0.01)
			{
				m_range_3000e[channelId] = 2e7;
				m_roundedRange[channelId] = 0.02;
			}
			else if(range_V > 0.005)
			{
				m_range_3000e[channelId] = 1e7;
				m_roundedRange[channelId] = 0.01;
			}
			else
			{
				m_range_3000e[channelId] = 5e6;
				m_roundedRange[channelId] = 0.005;
			}
			break;
	}

	//We need to allocate new buffers for this channel
	m_memDepthChanged = true;
	UpdateChannel(channelId);

	//Update trigger if this is the trigger channel.
	//Trigger is digital and threshold is specified in ADC counts.
	//We want to maintain constant trigger level in volts, not ADC counts.
	// !! this is done in UpdateChannel() already !!
	//if(m_triggerChannel == channelId)
	//	UpdateTrigger();
}

void PicoSCPIServer::SetAnalogOffset(size_t chIndex, double offset_V)
{
	lock_guard<mutex> lock(m_mutex);

	int channelId = chIndex & 0xff;

	double maxoff;
	double minoff;
	float maxoff_f;
	float minoff_f;

	//Clamp to allowed range
	switch(m_pico_type)
	{
		case PICO2000A:
			m_api.ps2000aGetAnalogueOffset(m_hScope, m_range_2000a[channelId], (PS2000A_COUPLING)m_coupling[channelId], &maxoff_f, &minoff_f);
			maxoff = maxoff_f;
			minoff = minoff_f;
			break;
		case PICO3000A:
			m_api.ps3000aGetAnalogueOffset(m_hScope, m_range_3000a[channelId], (PS3000A_COUPLING)m_coupling[channelId], &maxoff_f, &minoff_f);
			maxoff = maxoff_f;
			minoff = minoff_f;
			break;
		case PICO4000A:
			m_api.ps4000aGetAnalogueOffset(m_hScope, m_range[channelId], (PS4000A_COUPLING)m_coupling[channelId], &maxoff_f, &minoff_f);
			maxoff = maxoff_f;
			minoff = minoff_f;
			break;
		case PICO5000A:
			m_api.ps5000aGetAnalogueOffset(m_hScope, m_range_5000a[channelId], (PS5000A_COUPLING)m_coupling[channelId], &maxoff_f, &minoff_f);
			maxoff = maxoff_f;
			minoff = minoff_f;
			break;
		case PICO6000A:
			m_api.ps6000aGetAnalogueOffsetLimits(m_hScope, m_range[channelId], m_coupling[channelId], &maxoff, &minoff);
			break;
		case PICOPSOSPA:
			m_api.psospaGetAnalogueOffsetLimits(m_hScope, -m_range_3000e[channelId], m_range_3000e[channelId], PICO_X1_PROBE_NV, m_coupling[channelId], &maxoff, &minoff);
			break;
	}
	offset_V = min(maxoff, offset_V);
	offset_V = max(minoff, offset_V);

	m_offset[channelId] = offset_V;
	UpdateChannel(channelId);
}

void PicoSCPIServer::SetDigitalThreshold(size_t chIndex, double threshold_V)
{
	int channelId = chIndex & 0xff;
	int laneId = (chIndex >> 8) & 0xff;
	int16_t code = 0;

	switch(m_series)
	{
		case 2:
		case 3:
		case 5:
			//Threshold voltage range is 5V for MSO scopes
			code = round( (threshold_V * 32767) / 5.0);

			//Threshold voltage cannot be set individually, but only for each channel,
			//so we set the threshold value for all 8 lanes at once
			for(int i=0; i<7; i++)
				m_msoPodThreshold[channelId][i] = code;
			m_msoPodThresholdVoltage[channelId] = threshold_V;

			break;
		case 6:
			//Threshold voltage range is 8V for TA369 pods
			code = round( (threshold_V * 32767) / 8.0);
			m_msoPodThreshold[channelId][laneId] = code;
			m_msoPodThresholdVoltage[channelId] = threshold_V;
			break;
	}

	LogTrace("Setting MSO pod %d lane %d threshold to %f (code %d)\n", channelId, laneId, threshold_V, code);

	lock_guard<mutex> lock(m_mutex);

	//Update the pod if currently active
	if(m_msoPodEnabled[channelId])
		EnableMsoPod(channelId);
}

void PicoSCPIServer::SetDigitalHysteresis(size_t chIndex, double hysteresis)
{
	//Hysteresis is fixed on all devices with MSO option
	if( (m_series != 6) )
		return;

	lock_guard<mutex> lock(m_mutex);

	int channelId = chIndex & 0xff;

	//Calculate hysteresis
	int level = hysteresis;
	if(level <= 50)
		m_msoHysteresis[channelId] = PICO_LOW_50MV;
	else if(level <= 100)
		m_msoHysteresis[channelId] = PICO_NORMAL_100MV;
	else if(level <= 200)
		m_msoHysteresis[channelId] = PICO_HIGH_200MV;
	else
		m_msoHysteresis[channelId] = PICO_VERY_HIGH_400MV;

	LogTrace("Setting MSO pod %d hysteresis to %d mV (code %d)\n",
			 channelId, level, m_msoHysteresis[channelId]);

	//Update the pod if currently active
	if(m_msoPodEnabled[channelId])
		EnableMsoPod(channelId);
}

void PicoSCPIServer::SetSampleRate(uint64_t rate_hz)
{
	lock_guard<mutex> lock(m_mutex);
	int timebase = 0;
	//Convert sample rate to sample period
	m_sampleInterval = 1e15 / rate_hz;
	double period_ns = 1e9 / rate_hz;
	double clkdiv = period_ns / 0.2;

	switch(m_pico_type)
	{
		case PICO2000A:
			if(m_model.find("2205MSO") != string::npos)
			{
				if(period_ns < 5)
					timebase = 0;
				else
					timebase = round(100e6/rate_hz);
			}
			else if( m_model=="2206" || m_model=="2206A" || m_model=="2206B" || m_model=="2205AMSO" || m_model=="2405A" )
			{
				//!! 500 MS/s maximum sampling rate models
				if(period_ns < 4)
					timebase = 0;
				else if(period_ns < 16)
					timebase = round(log(5e8/rate_hz)/log(2));
				else
					timebase = round((625e5/rate_hz)+2);
			}
			else
			{
				//!! 1 GS/s maximum sampling rate models
				if(period_ns < 2)
					timebase = 0;
				else if(period_ns < 8)
					timebase = round(log(1e9/rate_hz)/log(2));
				else
					timebase = round((125e6/rate_hz)+2);
			}
			break;
		case PICO3000A:
			if( (m_model[1]=='2') and (m_model[4]=='A' or m_model[4]=='B') )
			{
				//!! A different implementation is needed for:
				//!!   PicoScope 3000A and 3000B Series 2-Channel USB 2.0 Oscilloscopes
				if(period_ns < 4)
					timebase = 0;
				else if(period_ns < 16)
					timebase = round(log(5e8/rate_hz)/log(2));
				else
					timebase = round((625e5/rate_hz)+2);
			}
			if( (m_model.find("MSO") != string::npos) and (m_model[4]!='D') )
			{
				//!! And another one for:
				//!!   PicoScope 3000 Series USB 2.0 MSOs
				if(period_ns < 4)
					timebase = 0;
				else if(period_ns < 8)
					timebase = round(log(5e8/rate_hz)/log(2));
				else
					timebase = round((125e6/rate_hz)+1);
			}
			else
			{
				//!! This part is applicable to the following devices:
				//!!   PicoScope 3000A and 3000B Series 4-Channel USB 2.0 Oscilloscopes
				//!!   PicoScope 3207A and 3207B USB 3.0 Oscilloscopes
				//!!   PicoScope 3000D Series USB 3.0 Oscilloscopes and MSOs
				if(period_ns < 2)
					timebase = 0;
				else if(period_ns < 8)
					timebase = round(log(1e9/rate_hz)/log(2));
				else
					timebase = round((125e6/rate_hz)+2);
			}
			break;
		case PICO4000A:
			if(m_model.find("4444") != string::npos)
			{
				if(period_ns < 5)
					timebase = 0;
				else if(period_ns < 40)
					timebase = round(log(4e8/rate_hz)/log(2));
				else
					timebase = round((50e6/rate_hz)+2);
			}
			else
				timebase = trunc((80e6/rate_hz)-1);
			break;
		case PICO5000A:
			switch(m_adcBits)
			{
				case 8:
				{
					if(period_ns < 2)
						timebase = 0;
					else if(period_ns < 8)
						timebase = round(log(1e9/rate_hz)/log(2));
					else
						timebase = round((125e6/rate_hz)+2);
					break;
				}

				case 12:
				{
					if(period_ns < 4)
						timebase = 1;
					else if(period_ns < 16)
						timebase = round(log(5e8/rate_hz)/log(2)+1);
					else
						timebase = round((625e5/rate_hz)+3);
					break;
				}

				case 14:
				case 15:
				{
					if(period_ns < 16)
						timebase = 3;
					else
						timebase = round((125e6/rate_hz)+2);
					break;
				}

				case 16:
				{
					if(period_ns < 32)
						timebase = 4;
					else
						timebase = round((625e5/rate_hz)+3);
					break;
				}
			}
			break;
		case PICO6000A:
			if(period_ns < 5)
				timebase = round(log(clkdiv)/log(2));
			else
				timebase = round(clkdiv/32) + 4;

			//6428E-D is calculated differently
			if(m_model[3] == '8')
			{
				if(clkdiv < 1)
					timebase = 0;
				else
					timebase = timebase + 1;
			}
			break;
		case PICOPSOSPA:
			m_sampleInterval = 1e15 / rate_hz;
			timebase = 1e12 / rate_hz;
			//LogError("SetSampleRate Error unknown m_series\n");
			break;
	}

	m_timebase = timebase;
	m_sampleRate = rate_hz;
	UpdateTrigger(); //TESTING lasse
}

void PicoSCPIServer::SetSampleDepth(uint64_t depth)
{
	lock_guard<mutex> lock(m_mutex);
	m_memDepth = depth;

	UpdateTrigger();
}

void PicoSCPIServer::SetTriggerDelay(uint64_t delay_fs)
{
	lock_guard<mutex> lock(m_mutex);

	m_triggerDelay = delay_fs;
	UpdateTrigger();
}

void PicoSCPIServer::SetTriggerSource(size_t chIndex)
{
	lock_guard<mutex> lock(m_mutex);

	auto type = GetChannelType(chIndex);
	switch(type)
	{
		case CH_ANALOG:
			m_triggerChannel = chIndex & 0xff;
			if(!m_channelOn[m_triggerChannel])
			{
				LogDebug("Trigger channel wasn't on, enabling it\n");
				m_channelOn[m_triggerChannel] = true;
				UpdateChannel(m_triggerChannel);
			}
			break;

		case CH_DIGITAL:
		{
			int npod = chIndex & 0xff;
			int nchan = (chIndex >> 8) & 0xff;
			m_triggerChannel = m_numChannels + npod*8 + nchan;

			if(!m_msoPodEnabled[npod])
			{
				LogDebug("Trigger pod wasn't on, enabling it\n");
				EnableMsoPod(npod);
			}
		}
		break;

		case CH_EXTERNAL_TRIGGER:
		{
			m_triggerChannel = PICO_TRIGGER_AUX;
			UpdateTrigger();
		};

		default:
			//TODO
			break;
	}

	bool wasOn = m_triggerArmed;
	Stop();

	UpdateTrigger();

	if(wasOn)
		StartCapture(false);
}

void PicoSCPIServer::SetTriggerLevel(double level_V)
{
	lock_guard<mutex> lock(m_mutex);

	m_triggerVoltage = level_V;
	UpdateTrigger();
}

void PicoSCPIServer::SetTriggerTypeEdge()
{
	//all triggers are edge, nothing to do here until we start supporting other trigger types
}

bool PicoSCPIServer::IsTriggerArmed()
{
	return m_triggerArmed;
}

void PicoSCPIServer::SetEdgeTriggerEdge(const string& edge)
{
	lock_guard<mutex> lock(m_mutex);

	if(edge == "RISING")
		m_triggerDirection = PICO_RISING;
	else if(edge == "FALLING")
		m_triggerDirection = PICO_FALLING;
	else if(edge == "ANY")
		m_triggerDirection = PICO_RISING_OR_FALLING;

	UpdateTrigger();
}

/**
	@brief Pushes channel configuration to the instrument
 */
void PicoSCPIServer::UpdateChannel(size_t chan)
{
	int16_t scaleVal;

	switch(m_pico_type)
	{
		case PICO2000A:
			m_api.ps2000aSetChannel(m_hScope, (PS2000A_CHANNEL)chan, m_channelOn[chan],
							  (PS2000A_COUPLING)m_coupling[chan], m_range_2000a[chan], -m_offset[chan]);
			m_api.ps2000aMaximumValue(m_hScope, &scaleVal);
			m_scaleValue = scaleVal;

			//We use software triggering based on raw ADC codes.
			//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
			//TODO: handle multi-input triggers
			if(chan == m_triggerChannel)
				UpdateTrigger();
			break;
		case PICO3000A:
			m_api.ps3000aSetChannel(m_hScope, (PS3000A_CHANNEL)chan, m_channelOn[chan],
							  (PS3000A_COUPLING)m_coupling[chan], m_range_3000a[chan], -m_offset[chan]);
			m_api.ps3000aSetBandwidthFilter(m_hScope, (PS3000A_CHANNEL)chan,
									  (PS3000A_BANDWIDTH_LIMITER)m_bandwidth_3000a[chan]);
			m_api.ps3000aMaximumValue(m_hScope, &scaleVal);
			m_scaleValue = scaleVal;

			//We use software triggering based on raw ADC codes.
			//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
			//TODO: handle multi-input triggers
			if(chan == m_triggerChannel)
				UpdateTrigger();
			break;
		case PICO4000A:
			m_api.ps4000aSetChannel(m_hScope, (PS4000A_CHANNEL)chan, m_channelOn[chan],
							  (PS4000A_COUPLING)m_coupling[chan], m_range[chan], -m_offset[chan]);
			m_api.ps4000aSetBandwidthFilter(m_hScope, (PS4000A_CHANNEL)chan,
									  (PS4000A_BANDWIDTH_LIMITER)m_bandwidth_5000a[chan]);
			m_api.ps4000aMaximumValue(m_hScope, &scaleVal);
			m_scaleValue = scaleVal;

			//We use software triggering based on raw ADC codes.
			//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
			//TODO: handle multi-input triggers
			if(chan == m_triggerChannel)
				UpdateTrigger();
			break;
		case PICO5000A:
			m_api.ps5000aSetChannel(m_hScope, (PS5000A_CHANNEL)chan, m_channelOn[chan],
							  (PS5000A_COUPLING)m_coupling[chan], m_range_5000a[chan], -m_offset[chan]);
			m_api.ps5000aSetBandwidthFilter(m_hScope, (PS5000A_CHANNEL)chan,
									  (PS5000A_BANDWIDTH_LIMITER)m_bandwidth_5000a[chan]);
			m_api.ps5000aMaximumValue(m_hScope, &scaleVal);
			m_scaleValue = scaleVal;

			//LogDebug(" - UpdateChannel %zu, range %f, scaleVal %d \n", chan, m_roundedRange[chan], scaleVal);

			//We use software triggering based on raw ADC codes.
			//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
			//TODO: handle multi-input triggers
			if(chan == m_triggerChannel)
				UpdateTrigger();
			break;
		case PICO6000A:
			if(m_channelOn[chan])
			{
				PICO_DEVICE_RESOLUTION currentRes;
				m_api.ps6000aSetChannelOn(m_hScope, (PICO_CHANNEL)chan,
									m_coupling[chan], m_range[chan], -m_offset[chan], m_bandwidth[chan]);
				m_api.ps6000aGetDeviceResolution(m_hScope, &currentRes);
				m_api.ps6000aGetAdcLimits(m_hScope, currentRes, 0, &scaleVal);
				m_scaleValue = scaleVal;

				//We use software triggering based on raw ADC codes.
				//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
				//TODO: handle multi-input triggers
				if(chan == m_triggerChannel)
					UpdateTrigger();
			}
			else
				m_api.ps6000aSetChannelOff(m_hScope, (PICO_CHANNEL)chan);
			break;
		case PICOPSOSPA:
			if(m_channelOn[chan])
			{
				PICO_DEVICE_RESOLUTION currentRes;
				m_api.psospaSetChannelOn(m_hScope, (PICO_CHANNEL)chan,
									m_coupling[chan], -m_range_3000e[chan], m_range_3000e[chan], PICO_X1_PROBE_NV, -m_offset[chan], m_bandwidth[chan]);
				m_api.psospaGetDeviceResolution(m_hScope, &currentRes);
				m_api.psospaGetAdcLimits(m_hScope, currentRes, 0, &scaleVal);
				m_scaleValue = scaleVal;

				//We use software triggering based on raw ADC codes.
				//Any time we change the frontend configuration on the trigger channel, it has to be reconfigured.
				//TODO: handle multi-input triggers
				if(chan == m_triggerChannel)
					UpdateTrigger();
			}
			else
				m_api.psospaSetChannelOff(m_hScope, (PICO_CHANNEL)chan);
			break;
	}
}

/**
	@brief Pushes trigger configuration to the instrument
 */
void PicoSCPIServer::UpdateTrigger(bool force)
{
	//Timeout, in microseconds, before initiating a trigger
	//Force trigger is really just a one-shot auto trigger with a 1us delay.
	uint32_t timeout = 0;
	if(force)
	{
		timeout = 1;
		m_lastTriggerWasForced = true;
		m_triggerOneShot = true;
	}
	else
		m_lastTriggerWasForced = false;

	bool triggerIsAnalog = (m_triggerChannel < m_numChannels) || (m_triggerChannel == PICO_TRIGGER_AUX);

	//Convert threshold from volts to ADC counts
	float offset = 0;
	if(triggerIsAnalog)
		offset = m_offset[m_triggerChannel];
	float scale = 1;
	if(triggerIsAnalog)
	{
		scale = m_roundedRange[m_triggerChannel] / 32767;
		if(scale == 0)
			scale = 1;
	}
	float trig_code = (m_triggerVoltage - offset) / scale;
	if(trig_code > 32767)
		trig_code = 32767;
	if(trig_code < -32767)
		trig_code = -32767;

	//This can happen early on during initialization.
	//Bail rather than dividing by zero.
	if(m_sampleInterval == 0)
		return;

	//Add delay before start of capture if needed
	int64_t triggerDelaySamples = m_triggerDelay / m_sampleInterval;
	uint64_t delay = 0;
	if(triggerDelaySamples < 0)
		delay = -triggerDelaySamples;

	switch(m_pico_type)
	{
		case PICO2000A:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				int ret = m_api.ps2000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS2000A_CHANNEL)PICO_TRIGGER_AUX,
							  trunc(trig_code), //0
							  (enPS2000AThresholdDirection)m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps2000aSetSimpleTrigger failed: %x\n", ret);
			}
			else if(m_triggerChannel < m_numChannels)
			{
				int ret = m_api.ps2000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS2000A_CHANNEL)m_triggerChannel,
							  trunc(trig_code),
							  (PS2000A_THRESHOLD_DIRECTION)m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps2000aSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				//Remove old trigger conditions
				m_api.ps2000aSetTriggerChannelConditions(
					m_hScope,
					NULL,
					0);

				//Set up new conditions
				int ntrig = m_triggerChannel - m_numChannels;
				//int trigpod = ntrig / 8;
				int triglane = ntrig % 8;
				PS2000A_TRIGGER_CONDITIONS cond;
				cond.digital = PS2000A_CONDITION_TRUE;
				//cond.external = PS2000A_CONDITION_FALSE;
				//cond.channelA = PS2000A_CONDITION_FALSE;
				//cond.channelB = PS2000A_CONDITION_FALSE;
				//cond.channelC = PS2000A_CONDITION_FALSE;
				//cond.channelD = PS2000A_CONDITION_FALSE;
				m_api.ps2000aSetTriggerChannelConditions(
					m_hScope,
					&cond,
					1);

				//Set up configuration on the selected channel
				PS2000A_DIGITAL_CHANNEL_DIRECTIONS dirs;
				dirs.channel = static_cast<PS2000A_DIGITAL_CHANNEL>(PS2000A_DIGITAL_CHANNEL_0 + triglane);
				dirs.direction = PS2000A_DIGITAL_DIRECTION_RISING;				//TODO: configurable
				m_api.ps2000aSetTriggerDigitalPortProperties(
					m_hScope,
					&dirs,
					1);

				//m_api.ps6000aSetTriggerDigitalPortProperties doesn't have a timeout!
				//Should we call m_api.ps6000aSetTriggerChannelProperties with no elements to do this?
				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is digital\n");
			}
			break;
		case PICO3000A:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				/* TODO PICO_TRIGGER_AUX PICO3000A similarly to PICO6000A... */
				/*
				//Seems external trigger only supports zero crossing???
				trig_code = 0;

				//Remove old trigger conditions
				m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					NULL,
					0,
					PICO_CLEAR_ALL);

				//Set up new conditions
				PICO_CONDITION cond;
				cond.source = PICO_TRIGGER_AUX;
				cond.condition = PICO_CONDITION_TRUE;
				int ret = m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					&cond,
					1,
					PICO_ADD);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelConditions failed: %x\n", ret);

				PICO_DIRECTION dir;
				dir.channel = PICO_TRIGGER_AUX;
				dir.direction = PICO_RISING;
				dir.thresholdMode = PICO_LEVEL;
				ret = m_api.ps6000aSetTriggerChannelDirections(
					m_hScope,
					&dir,
					1);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelDirections failed: %x\n", ret);

				PICO_TRIGGER_CHANNEL_PROPERTIES prop;
				prop.thresholdUpper = trig_code;
				prop.thresholdUpperHysteresis = 32;
				prop.thresholdLower = 0;
				prop.thresholdLowerHysteresis = 0;
				prop.channel = PICO_TRIGGER_AUX;
				ret = m_api.ps6000aSetTriggerChannelProperties(
					m_hScope,
					&prop,
					1,
					0,
					0);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelProperties failed: %x\n", ret);

				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is external\n");
				*/
				/* API is same as 6000a API */
				int ret = m_api.ps3000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS3000A_CHANNEL)PICO_TRIGGER_AUX,
							  trunc(trig_code), //0
							  (PS3000A_THRESHOLD_DIRECTION)m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps6000aSetSimpleTrigger failed: %x\n", ret);
			}
			else if(m_triggerChannel < m_numChannels)
			{
				/* API is same as 6000a API */
				int ret = m_api.ps3000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS3000A_CHANNEL)m_triggerChannel,
							  round(trig_code),
							  (PS3000A_THRESHOLD_DIRECTION)m_triggerDirection, // same as 6000a api
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps3000aSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				//Remove old trigger conditions
				m_api.ps3000aSetTriggerChannelConditionsV2(
					m_hScope,
					NULL,
					0);

				//Set up new conditions
				int ntrig = m_triggerChannel - m_numChannels;
				//int trigpod = ntrig / 8;
				int triglane = ntrig % 8;
				PS3000A_TRIGGER_CONDITIONS_V2 cond;
				cond.digital = PS3000A_CONDITION_TRUE;
				//cond.external = PS3000A_CONDITION_FALSE;
				//cond.channelA = PS3000A_CONDITION_FALSE;
				//cond.channelB = PS3000A_CONDITION_FALSE;
				//cond.channelC = PS3000A_CONDITION_FALSE;
				//cond.channelD = PS3000A_CONDITION_FALSE;
				m_api.ps3000aSetTriggerChannelConditionsV2(
					m_hScope,
					&cond,
					1);

				//Set up configuration on the selected channel
				PS3000A_DIGITAL_CHANNEL_DIRECTIONS dirs;
				dirs.channel = static_cast<PS3000A_DIGITAL_CHANNEL>(PS3000A_DIGITAL_CHANNEL_0 + triglane);
				dirs.direction = PS3000A_DIGITAL_DIRECTION_RISING;				//TODO: configurable
				m_api.ps3000aSetTriggerDigitalPortProperties(
					m_hScope,
					&dirs,
					1);

				//m_api.ps6000aSetTriggerDigitalPortProperties doesn't have a timeout!
				//Should we call m_api.ps6000aSetTriggerChannelProperties with no elements to do this?
				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is digital\n");
			}
			break;
		case PICO4000A:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				LogError("PS4000 has no external trigger input\n");
			}
			else if(m_triggerChannel < m_numChannels)
			{
				/* API is same as 6000a API */
				int ret = m_api.ps4000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS4000A_CHANNEL)m_triggerChannel,
							  round(trig_code),
							  (PS4000A_THRESHOLD_DIRECTION)m_triggerDirection, // same as 6000a api
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps4000aSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				LogError("PS4000 has no digital trigger option\n");
			}
			break;
		case PICO5000A:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				int ret = m_api.ps5000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS5000A_CHANNEL)PICO_TRIGGER_AUX,
							  trunc(trig_code), //0
							  (PS5000A_THRESHOLD_DIRECTION)m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps5000aSetSimpleTrigger failed: %x\n", ret);
			}
			else if(m_triggerChannel < m_numChannels)
			{
				/* API is same as 6000a API */
				int ret = m_api.ps5000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PS5000A_CHANNEL)m_triggerChannel,
							  trunc(trig_code),
							  (PS5000A_THRESHOLD_DIRECTION)m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps5000aSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				//Remove old trigger conditions
				m_api.ps5000aSetTriggerChannelConditionsV2(
					m_hScope,
					NULL,
					0,
					PS5000A_CLEAR);

				//Set up new conditions
				int ntrig = m_triggerChannel - m_numChannels;
				int trigpod = ntrig / 8;
				int triglane = ntrig % 8;
				PS5000A_CONDITION cond;
				cond.source = static_cast<PS5000A_CHANNEL>(PS5000A_DIGITAL_PORT0 + trigpod);
				cond.condition = PS5000A_CONDITION_TRUE;
				m_api.ps5000aSetTriggerChannelConditionsV2(
					m_hScope,
					&cond,
					1,
					PS5000A_ADD);

				//Set up configuration on the selected channel
				PS5000A_DIGITAL_CHANNEL_DIRECTIONS dirs;
				dirs.channel = static_cast<PS5000A_DIGITAL_CHANNEL>(PS5000A_DIGITAL_CHANNEL_0 + triglane);
				dirs.direction = PS5000A_DIGITAL_DIRECTION_RISING;				//TODO: configurable
				m_api.ps5000aSetTriggerDigitalPortProperties(
					m_hScope,
					&dirs,
					1);

				//m_api.ps6000aSetTriggerDigitalPortProperties doesn't have a timeout!
				//Should we call m_api.ps6000aSetTriggerChannelProperties with no elements to do this?
				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is digital\n");
			}
			break;
		case PICO6000A:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				/*
				//Seems external trigger only supports zero crossing???
				trig_code = 0;

				//Remove old trigger conditions
				m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					NULL,
					0,
					PICO_CLEAR_ALL);

				//Set up new conditions
				PICO_CONDITION cond;
				cond.source = PICO_TRIGGER_AUX;
				cond.condition = PICO_CONDITION_TRUE;
				int ret = m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					&cond,
					1,
					PICO_ADD);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelConditions failed: %x\n", ret);

				PICO_DIRECTION dir;
				dir.channel = PICO_TRIGGER_AUX;
				dir.direction = PICO_RISING;
				dir.thresholdMode = PICO_LEVEL;
				ret = m_api.ps6000aSetTriggerChannelDirections(
					m_hScope,
					&dir,
					1);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelDirections failed: %x\n", ret);

				PICO_TRIGGER_CHANNEL_PROPERTIES prop;
				prop.thresholdUpper = trig_code;
				prop.thresholdUpperHysteresis = 32;
				prop.thresholdLower = 0;
				prop.thresholdLowerHysteresis = 0;
				prop.channel = PICO_TRIGGER_AUX;
				ret = m_api.ps6000aSetTriggerChannelProperties(
					m_hScope,
					&prop,
					1,
					0,
					0);
				if(ret != PICO_OK)
					LogError("ps6000aSetTriggerChannelProperties failed: %x\n", ret);

				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is external\n");
				*/

				int ret = m_api.ps6000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  PICO_TRIGGER_AUX,
							  0, //Fixed threshold (2.5V CMOS Hi-Z)
							  m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps6000aSetSimpleTrigger failed: %x\n", ret);
			}
			else if(m_triggerChannel < m_numChannels)
			{
				int ret = m_api.ps6000aSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PICO_CHANNEL)m_triggerChannel,
							  round(trig_code),
							  m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("ps6000aSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				//Remove old trigger conditions
				m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					NULL,
					0,
					PICO_CLEAR_ALL);

				//Set up new conditions
				int ntrig = m_triggerChannel - m_numChannels;
				int trigpod = ntrig / 8;
				int triglane = ntrig % 8;
				PICO_CONDITION cond;
				cond.source = static_cast<PICO_CHANNEL>(PICO_PORT0 + trigpod);
				cond.condition = PICO_CONDITION_TRUE;
				m_api.ps6000aSetTriggerChannelConditions(
					m_hScope,
					&cond,
					1,
					PICO_ADD);

				//Set up configuration on the selected channel
				PICO_DIGITAL_CHANNEL_DIRECTIONS dirs;
				dirs.channel = static_cast<PICO_PORT_DIGITAL_CHANNEL>(PICO_PORT_DIGITAL_CHANNEL0 + triglane);
				dirs.direction = PICO_DIGITAL_DIRECTION_RISING;				//TODO: configurable
				m_api.ps6000aSetTriggerDigitalPortProperties(
					m_hScope,
					cond.source,
					&dirs,
					1);

				//m_api.ps6000aSetTriggerDigitalPortProperties doesn't have a timeout!
				//Should we call m_api.ps6000aSetTriggerChannelProperties with no elements to do this?
				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is digital\n");
			}
			break;
		case PICOPSOSPA:
			if(m_triggerChannel == PICO_TRIGGER_AUX)
			{
				int ret = m_api.psospaSetSimpleTrigger(
							  m_hScope,
							  1,
							  PICO_TRIGGER_AUX,
							  0, //Fixed threshold (3.3V CMOS Hi-Z)
							  m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("psospaSetSimpleTrigger failed: %x\n", ret);
			}
			else if(m_triggerChannel < m_numChannels)
			{
				int ret = m_api.psospaSetSimpleTrigger(
							  m_hScope,
							  1,
							  (PICO_CHANNEL)m_triggerChannel,
							  round(trig_code),
							  m_triggerDirection,
							  delay,
							  timeout);
				if(ret != PICO_OK)
					LogError("psospaSetSimpleTrigger failed: %x\n", ret);
			}
			else
			{
				//Remove old trigger conditions
				m_api.psospaSetTriggerChannelConditions(
					m_hScope,
					NULL,
					0,
					PICO_CLEAR_ALL);

				//Set up new conditions
				int ntrig = m_triggerChannel - m_numChannels;
				int trigpod = ntrig / 8;
				int triglane = ntrig % 8;
				PICO_CONDITION cond;
				cond.source = static_cast<PICO_CHANNEL>(PICO_PORT0 + trigpod);
				cond.condition = PICO_CONDITION_TRUE;
				m_api.psospaSetTriggerChannelConditions(
					m_hScope,
					&cond,
					1,
					PICO_ADD);

				//Set up configuration on the selected channel
				PICO_DIGITAL_CHANNEL_DIRECTIONS dirs;
				dirs.channel = static_cast<PICO_PORT_DIGITAL_CHANNEL>(PICO_PORT_DIGITAL_CHANNEL0 + triglane);
				dirs.direction = PICO_DIGITAL_DIRECTION_RISING;				//TODO: configurable
				m_api.psospaSetTriggerDigitalPortProperties(
					m_hScope,
					cond.source,
					&dirs,
					1);

				//m_api.psospaSetTriggerDigitalPortProperties doesn't have a timeout!
				//Should we call m_api.psospaSetTriggerChannelProperties with no elements to do this?
				if(force)
					LogWarning("Force trigger doesn't currently work if trigger source is digital\n");
			}
			break;
	}

	if(m_triggerArmed)
		StartCapture(true);
}

void PicoSCPIServer::Stop()
{
	switch(m_pico_type)
	{
		case PICO2000A:
			m_api.ps2000aStop(m_hScope);
			break;
		case PICO3000A:
			m_api.ps3000aStop(m_hScope);
			break;
		case PICO4000A:
			m_api.ps4000aStop(m_hScope);
			break;
		case PICO5000A:
			m_api.ps5000aStop(m_hScope);
			break;
		case PICO6000A:
			m_api.ps6000aStop(m_hScope);
			break;
		case PICOPSOSPA:
			m_api.psospaStop(m_hScope);
			break;
	}
}

PICO_STATUS PicoSCPIServer::StartInternal()
{
	//Calculate pre/post trigger time configuration based on trigger delay
	int64_t triggerDelaySamples = m_triggerDelay / m_sampleInterval;
	size_t nPreTrigger = min(max(triggerDelaySamples, (int64_t)0L), (int64_t)m_memDepth);
	size_t nPostTrigger = m_memDepth - nPreTrigger;
	int32_t nPreTrigger_int = nPreTrigger;
	int32_t nPostTrigger_int = nPostTrigger;
	m_triggerSampleIndex = nPreTrigger;

	switch(m_pico_type)
	{
		case PICO2000A:
			return m_api.ps2000aRunBlock(m_hScope, nPreTrigger_int, nPostTrigger_int, m_timebase, 1, NULL, 0, NULL, NULL);
			break;
		case PICO3000A:
			return m_api.ps3000aRunBlock(m_hScope, nPreTrigger_int, nPostTrigger_int, m_timebase, 1, NULL, 0, NULL, NULL);
			break;
		case PICO4000A:
			return m_api.ps4000aRunBlock(m_hScope, nPreTrigger_int, nPostTrigger_int, m_timebase, NULL, 0, NULL, NULL);
			break;
		case PICO5000A:
			return m_api.ps5000aRunBlock(m_hScope, nPreTrigger_int, nPostTrigger_int, m_timebase, NULL, 0, NULL, NULL);
			break;
		case PICO6000A:
			return m_api.ps6000aRunBlock(m_hScope, nPreTrigger, nPostTrigger, m_timebase, NULL, 0, NULL, NULL);
			break;
		case PICOPSOSPA:
			return m_api.psospaRunBlock(m_hScope, nPreTrigger, nPostTrigger, m_timebase, NULL, 0, NULL, NULL);
			break;
		default:
			//return PICO_OK;
			return PICO_CANCELLED;
	}
}

void PicoSCPIServer::StartCapture(bool stopFirst, bool force)
{
	//If previous trigger was forced, we need to reconfigure the trigger to be not-forced now
	if(m_lastTriggerWasForced && !force)
	{
		m_triggerOneShot = false;
		Stop();
		UpdateTrigger();
	}

	m_offsetDuringArm = m_offset;
	m_channelOnDuringArm = m_channelOn;
	for(size_t i=0; i<m_numDigitalPods; i++)
		m_msoPodEnabledDuringArm[i] = m_msoPodEnabled[i];
	if(m_captureMemDepth != m_memDepth)
		m_memDepthChanged = true;
	m_captureMemDepth = m_memDepth;
	m_sampleIntervalDuringArm = m_sampleInterval;

	LogTrace("StartCapture stopFirst %d memdepth %zu\n", stopFirst, m_captureMemDepth);

	PICO_STATUS status;
	status = PICO_RESERVED_1;
	if(stopFirst)
		Stop();
	status = StartInternal();

	//not sure why this happens...
	while(status == PICO_HARDWARE_CAPTURING_CALL_STOP)
	{
		//Not sure what causes this, but seems to be harmless?
		//Demote to trace for now
		LogTrace("Got PICO_HARDWARE_CAPTURING_CALL_STOP (but scope should have been stopped already)\n");

		Stop();
		status = StartInternal();
	}

	//Don't choke if we couldn't start the block
	if(status != PICO_OK)
	{
		LogWarning("psXXXXRunBlock failed, code %d / 0x%x\n", status, status);
		m_triggerArmed = false;
		return;
	}

	m_triggerArmed = true;
}

bool PicoSCPIServer::EnableMsoPod(size_t npod)
{
	m_msoPodEnabled[npod] = true;
	uint32_t status = PICO_OK;

	switch(m_pico_type)
	{
		case PICO2000A:
		{
			PS2000A_DIGITAL_PORT podId = (PS2000A_DIGITAL_PORT)(PS2000A_DIGITAL_PORT0 + npod);
			status = m_api.ps2000aSetDigitalPort(m_hScope, (PS2000A_DIGITAL_PORT)podId, 1, m_msoPodThreshold[npod][0]);
			if(status != PICO_OK)
			{
				LogError("ps2000aSetDigitalPort failed with code %x\n", status);
				return false;
			}
			break;
		}
		case PICO3000A:
		{
			PS3000A_DIGITAL_PORT podId = (PS3000A_DIGITAL_PORT)(PS3000A_DIGITAL_PORT0 + npod);
			status = m_api.ps3000aSetDigitalPort(m_hScope, (PS3000A_DIGITAL_PORT)podId, 1, m_msoPodThreshold[npod][0]);
			if(status != PICO_OK)
			{
				LogError("ps3000aSetDigitalPort failed with code %x\n", status);
				return false;
			}
			break;
		}
		case PICO4000A:
			break;
		case PICO5000A:
		{
			PS5000A_CHANNEL podId = (PS5000A_CHANNEL)(PS5000A_DIGITAL_PORT0 + npod);
			status = m_api.ps5000aSetDigitalPort(m_hScope, (PS5000A_CHANNEL)podId, 1, m_msoPodThreshold[npod][0]);
			LogTrace("ps5000aSetDigitalPort Threshold: %i \n", m_msoPodThreshold[npod][0]);
			if(status != PICO_OK)
			{
				LogError("ps5000aSetDigitalPort failed with code %x\n", status);
				return false;
			}
			break;
		}
		case PICO6000A:
		{
			PICO_CHANNEL podId = (PICO_CHANNEL)(PICO_PORT0 + npod);
			status = m_api.ps6000aSetDigitalPortOn(
							  m_hScope,
							  podId,
							  m_msoPodThreshold[npod],
							  8,
							  m_msoHysteresis[npod]);
			if(status != PICO_OK)
			{
				LogError("ps6000aSetDigitalPortOn failed with code %x\n", status);
				return false;
			}
			break;
		}
		case PICOPSOSPA:
		{
			PICO_CHANNEL podId = (PICO_CHANNEL)(PICO_PORT0 + npod);
			status = m_api.psospaSetDigitalPortOn(
							  m_hScope,
							  podId,
							  m_msoPodThresholdVoltage[npod]);
			if(status != PICO_OK)
			{
				LogError("psospaSetDigitalPortOn failed with code %x\n", status);
				return false;
			}
			break;
		}
	}
	return true;
}

void PicoSCPIServer::GenerateSquareWave(int16_t* &waveform, size_t bufferSize, double dutyCycle, int16_t amplitude)
{
	// Validate inputs
	if (!waveform || bufferSize == 0)
	{
		LogError("GenerateSquareWave has Invalid input \n");
	}

	// Calculate number of high samples based on duty cycle
	size_t highSamples = static_cast<size_t>(bufferSize * (dutyCycle / 100.0));

	// Generate square wave
	for (size_t i = 0; i < bufferSize; i++)
	{
		if (i < highSamples)
		{
			waveform[i] = amplitude;     // High level
		}
		else
		{
			waveform[i] = -amplitude;    // Low level
		}
	}
}


}

#endif
