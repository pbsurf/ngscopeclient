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
	@brief Waveform data thread (data plane traffic only, no control plane SCPI)
 */
#ifdef HAS_PICO

#include "picobridge.h"
#include "PicoSCPIServer.h"
#include <string.h>
#include <stdarg.h>
#include <stdexcept>

using namespace std;

namespace picobridge
{

/**
	@brief Ends the waveform thread after an SDK error

	ps6000d called LogFatal() here, which would end ngscopeclient too.
 */
[[noreturn]] static void WaveformThreadFatal(const char* format, ...)
{
	char buf[256];
	va_list va;
	va_start(va, format);
	vsnprintf(buf, sizeof(buf), format, va);
	va_end(va);
	throw runtime_error(buf);
}

/**
	@brief Reads any ACKs the client has sent

	@param wait		If true, wait (up to 10 ms) for one to arrive if there are none
 */
void PicoSCPIServer::CheckForACKs(bool wait)
{
	if(wait && (m_ackPipe.BytesAvailable() < 4))
		m_ackPipe.WaitForData(10);

	while(m_ackPipe.BytesAvailable() >= 4)
	{
		if(!m_ackPipe.Read((uint8_t*)&m_lastRxAck, sizeof(m_lastRxAck), 1000))
			break;
		LogTrace("Got ACK %u\n", m_lastRxAck);
	}
}

void PicoSCPIServer::WaveformServerThread()
{
#ifdef __linux__
	pthread_setname_np(pthread_self(), "WaveformThread");
#endif

	m_lastTxSeq = 0;
	m_lastRxAck = 0;

	//Set up channel IDs
	m_channelIDs.clear();
	for(size_t i=0; i<m_numChannels; i++)
		m_channelIDs.push_back((PICO_CHANNEL)i);
	for(size_t i=0; i<m_numDigitalPods; i++)
		m_channelIDs.push_back((PICO_CHANNEL)(PICO_PORT0 + i));

	map<size_t, int16_t*> waveformBuffers;
	uint64_t numSamples = 0;
	uint32_t numSamples_int = 0;
	uint16_t numchans;
	try
	{
		while(!m_waveformThreadQuit)
		{
			int16_t ready;
			{
				lock_guard<mutex> lock(m_mutex);
				switch(m_pico_type)
				{
					case PICO2000A:
						m_api.ps2000aIsReady(m_hScope, &ready);
						break;
					case PICO3000A:
						m_api.ps3000aIsReady(m_hScope, &ready);
						break;
					case PICO4000A:
						m_api.ps4000aIsReady(m_hScope, &ready);
						break;
					case PICO5000A:
						m_api.ps5000aIsReady(m_hScope, &ready);
						break;
					case PICO6000A:
						m_api.ps6000aIsReady(m_hScope, &ready);
						break;
					case PICOPSOSPA:
						m_api.psospaIsReady(m_hScope, &ready);
						break;
				}
			}

			if( (ready == 0) || (!m_triggerArmed) )
			{
				std::this_thread::sleep_for(std::chrono::microseconds(1000));
				continue;
			}

			size_t interval;
			map<size_t, bool> channelOn;
			bool msoPodEnabled[2];
			float trigphase = 0;
			map<size_t, float> scales;
			map<size_t, float> offsets;
			{
				lock_guard<mutex> lock(m_mutex);

				interval = m_sampleIntervalDuringArm;
				channelOn = m_channelOnDuringArm;
				msoPodEnabled[0] = m_msoPodEnabledDuringArm[0];
				msoPodEnabled[1] = m_msoPodEnabledDuringArm[1];

				//Stop the trigger
				PICO_STATUS status = PICO_OPERATION_FAILED;
				switch(m_pico_type)
				{
					case PICO2000A:
						status = m_api.ps2000aStop(m_hScope);
						break;
					case PICO3000A:
						status = m_api.ps3000aStop(m_hScope);
						break;
					case PICO4000A:
						status = m_api.ps4000aStop(m_hScope);
						break;
					case PICO5000A:
						status = m_api.ps5000aStop(m_hScope);
						break;
					case PICO6000A:
						status = m_api.ps6000aStop(m_hScope);
						break;
					case PICOPSOSPA:
						status = m_api.psospaStop(m_hScope);
						break;
				}
				if(PICO_OK != status)
					WaveformThreadFatal("psXXXXStop failed (code 0x%x)\n", status);

				//Verify it's actually stopped

				//Set up buffers if needed
				if(m_memDepthChanged || waveformBuffers.empty())
				{
					//LogVerbose("Reallocating buffers\n");

					//Clear out old buffers
					for(auto ch : m_channelIDs)
					{
						switch(m_pico_type)
						{
							case PICO2000A:
								m_api.ps2000aSetDataBuffer(m_hScope, (PS2000A_CHANNEL)ch, NULL,
													0, 0, PS2000A_RATIO_MODE_NONE);
								break;
							case PICO3000A:
								m_api.ps3000aSetDataBuffer(m_hScope, (PS3000A_CHANNEL)ch, NULL,
													0, 0, PS3000A_RATIO_MODE_NONE);
								break;
							case PICO4000A:
								m_api.ps4000aSetDataBuffer(m_hScope, (PS4000A_CHANNEL)ch, NULL,
													0, 0, PS4000A_RATIO_MODE_NONE);
								break;
							case PICO5000A:
								m_api.ps5000aSetDataBuffer(m_hScope, (PS5000A_CHANNEL)ch, NULL,
													0, 0, PS5000A_RATIO_MODE_NONE);
								break;
							case PICO6000A:
								m_api.ps6000aSetDataBuffer(m_hScope, ch, NULL,
													0, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_CLEAR_ALL);
								break;
							case PICOPSOSPA:
								m_api.psospaSetDataBuffer(m_hScope, ch, NULL,
													0, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_CLEAR_ALL);
								break;
						}
					}

					//Clear out old buffers
					for(size_t i=0; i<m_channelIDs.size(); i++)
					{
						if(waveformBuffers[i])
						{
							delete[] waveformBuffers[i];
							waveformBuffers[i] = NULL;
						}
					}

					//Set up new ones
					//TODO: Only allocate memory if the channel is actually enabled
					for(size_t i=0; i<m_channelIDs.size(); i++)
					{
						//Allocate memory if needed
						waveformBuffers[i] = new int16_t[m_captureMemDepth];
						memset(waveformBuffers[i], 0x00, m_captureMemDepth * sizeof(int16_t));

						//Give it to the scope, removing any other buffer we might have
						auto ch = m_channelIDs[i];
						switch(m_pico_type)
						{
							case PICO2000A:
								status = m_api.ps2000aSetDataBuffer(m_hScope, (PS2000A_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, 0, PS2000A_RATIO_MODE_NONE);
								break;
							case PICO3000A:
								status = m_api.ps3000aSetDataBuffer(m_hScope, (PS3000A_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, 0, PS3000A_RATIO_MODE_NONE);
								break;
							case PICO4000A:
								status = m_api.ps4000aSetDataBuffer(m_hScope, (PS4000A_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, 0, PS4000A_RATIO_MODE_NONE);
								break;
							case PICO5000A:
								status = m_api.ps5000aSetDataBuffer(m_hScope, (PS5000A_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, 0, PS5000A_RATIO_MODE_NONE);
								break;
							case PICO6000A:
								status = m_api.ps6000aSetDataBuffer(m_hScope, (PICO_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
								break;
							case PICOPSOSPA:
								status = m_api.psospaSetDataBuffer(m_hScope, (PICO_CHANNEL)ch, waveformBuffers[i],
															m_captureMemDepth, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
								break;
						}
						if(status != PICO_OK)
							WaveformThreadFatal("psXXXXSetDataBuffer for channel %d failed (code 0x%x)\n", ch, status);
					}

					m_memDepthChanged = false;
				}

				//Download the data from the scope
				numSamples = m_captureMemDepth;
				numSamples_int = m_captureMemDepth;
				int16_t overflow = 0;
				switch(m_pico_type)
				{
					case PICO2000A:
						status = m_api.ps2000aGetValues(m_hScope, 0, &numSamples_int, 1, PS2000A_RATIO_MODE_NONE, 0, &overflow);
						numSamples = numSamples_int;
						break;
					case PICO3000A:
						status = m_api.ps3000aGetValues(m_hScope, 0, &numSamples_int, 1, PS3000A_RATIO_MODE_NONE, 0, &overflow);
						numSamples = numSamples_int;
						break;
					case PICO4000A:
						status = m_api.ps4000aGetValues(m_hScope, 0, &numSamples_int, 1, PS4000A_RATIO_MODE_NONE, 0, &overflow);
						numSamples = numSamples_int;
						break;
					case PICO5000A:
						status = m_api.ps5000aGetValues(m_hScope, 0, &numSamples_int, 1, PS5000A_RATIO_MODE_NONE, 0, &overflow);
						numSamples = numSamples_int;
						break;
					case PICO6000A:
						status = m_api.ps6000aGetValues(m_hScope, 0, &numSamples, 1, PICO_RATIO_MODE_RAW, 0, &overflow);
						break;
					case PICOPSOSPA:
						status = m_api.psospaGetValues(m_hScope, 0, &numSamples, 1, PICO_RATIO_MODE_RAW, 0, &overflow);
						break;
				}
				if(status == PICO_NO_SAMPLES_AVAILABLE)
				{
					LogVerbose("PICO_NO_SAMPLES_AVAILABLE\n");
					//This response will occur if some setting like vertical scale changed just before aGetValues.
					//flush buffers and update channel
					m_memDepthChanged = true;
					UpdateTrigger(true);
					continue;
				}
				if(PICO_OK != status)
					WaveformThreadFatal("psXXXXGetValues (code 0x%x)\n", status);

				//Figure out how many channels are active in this capture
				numchans = 0;
				for(size_t i=0; i<m_numChannels; i++)
				{
					if(m_channelOnDuringArm[i])
						numchans ++;
				}
				for(size_t i=0; i<m_numDigitalPods; i++)
				{
					if(m_msoPodEnabledDuringArm[i])
						numchans ++;
				}

				//Work out what the headers need now, since re-arming below updates the "during arm" settings
				//Interpolate trigger position if we're using an analog level trigger
				bool triggerIsAnalog = (m_triggerChannel < m_numChannels);
				if(triggerIsAnalog)
					trigphase = InterpolateTriggerTime(waveformBuffers[m_triggerChannel]);
				for(size_t i=0; i<m_numChannels; i++)
				{
					scales[i] = m_roundedRange[i] / m_scaleValue;
					offsets[i] = m_offsetDuringArm[i];
				}

				//Re-arm the trigger if doing repeating triggers.
				//ps6000d did this after sending the data. Doing it first lets the next capture run while the client
				//takes this one. The buffers are free again by the next GetValues(), since writes to the pipe wait
				//until the client has copied the data out.
				if(m_triggerOneShot)
				{
					m_triggerArmed = false;
				}
				else
				{
					if(m_captureMemDepth != m_memDepth)
						m_memDepthChanged = true;

					//Restart
					StartCapture(false);
				}
			}

			//Do *not* hold mutex while sending data to the client
			//This can take a long time and we don't want to block the control channel
			{
				//Bump sequence number
				m_lastTxSeq ++;

				#pragma pack(push, 1)
				struct
				{
					//Sequence number
					uint32_t sequence;

					//Number of channels in the current waveform
					uint16_t numChannels;

					//Sample interval.
					//May be different from m_srate if we changed the rate after the trigger was armed
					int64_t fs_per_sample;
				} wfmhdrs;
				#pragma pack(pop)
				wfmhdrs.sequence = m_lastTxSeq;
				wfmhdrs.numChannels = numchans;
				wfmhdrs.fs_per_sample = interval;

				//Send the top level waveform headers
				//TODO: send overflow flags to client
				if(!m_dataPipe.Write((uint8_t*)&wfmhdrs, sizeof(wfmhdrs)))
					break;

				//Process incoming ACKs
				CheckForACKs(false);

				//Backpressure if we have too many waveforms in flight
				const int maxWaveformsInFlight = 5;
				while( ((m_lastTxSeq - m_lastRxAck) >= maxWaveformsInFlight) && !m_waveformThreadQuit)
					CheckForACKs(true);

				//Send data for each channel to the client
				for(size_t i=0; i<m_channelIDs.size(); i++)
				{
					//Analog channels
					if((i < m_numChannels) && (channelOn[i]) )
					{
						#pragma pack(push, 1)
						struct
						{
							size_t nchan;
							size_t numSamples;
							float scale;
							float offset;
							float trigphase;
						} chdrs;
						#pragma pack(pop)

						chdrs.nchan = i;
						chdrs.numSamples = numSamples;
						chdrs.scale = scales[i];
						chdrs.offset = offsets[i];
						chdrs.trigphase = trigphase;

						//Send channel headers
						if(!m_dataPipe.Write((uint8_t*)&chdrs, sizeof(chdrs)))
							break;

						//Send the actual waveform data
						if(!m_dataPipe.Write((uint8_t*)waveformBuffers[i], numSamples * sizeof(int16_t)))
							break;
					}

					//Digital channels
					else if( (i >= m_numChannels) && (msoPodEnabled[i - m_numChannels]) )
					{
						#pragma pack(push, 1)
						struct
						{
							size_t nchan;
							size_t numSamples;
							float trigphase;
						} chdrs;
						#pragma pack(pop)
						chdrs.nchan = i;
						chdrs.numSamples = numSamples;
						chdrs.trigphase = trigphase;

						if(!m_dataPipe.Write((uint8_t*)&chdrs, sizeof(chdrs)))
							break;
						if(!m_dataPipe.Write((uint8_t*)waveformBuffers[i], numSamples * sizeof(int16_t)))
							break;
					}
				}
			}

		}
	}
	catch(const exception& e)
	{
		LogError("Pico waveform thread stopped: %s", e.what());
	}

	LogDebug("Client disconnected from data plane socket\n");
	{
		lock_guard<mutex> lock(m_mutex);
		Stop();
	}

	//Clean up temporary buffers
	for(auto it : waveformBuffers)
		delete[] it.second;
}

float PicoSCPIServer::InterpolateTriggerTime(int16_t* buf)
{
	if(m_triggerSampleIndex <= 0)
		return 0;

	//trigger scale value depends on ADC setting and is different for EXT trig input
	size_t trigmaxcount = m_scaleValue;
	if(m_triggerChannel == PICO_TRIGGER_AUX)
	{
		//set model dependent EXT trig scale value
		trigmaxcount = 32767;
		if(m_series == 6)
			trigmaxcount = 32512;
	}

	float trigscale = m_roundedRange[m_triggerChannel] / trigmaxcount;
	float trigoff = m_offsetDuringArm[m_triggerChannel];

	float fa = buf[m_triggerSampleIndex-1] * trigscale + trigoff;
	float fb = buf[m_triggerSampleIndex] * trigscale + trigoff;

	//no need to divide by time, sample spacing is normalized to 1 timebase unit
	float slope = (fb - fa);
	float delta = m_triggerVoltage - fa;
	if(slope == 0)
		return 0;
	else
		return delta / slope;
}

}

#endif
