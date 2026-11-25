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
	@brief Declaration of PicoSCPIServer
	@ingroup picobridge
 */

#ifndef picobridge_PicoSCPIServer_h
#define picobridge_PicoSCPIServer_h

#include "BridgeSCPIServer.h"
#include "PicoApi.h"
#include "PicoDataPipe.h"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace picobridge
{

enum PicoScopeType
{
	PICO2000A,
	PICO3000A,
	PICO4000A,
	PICO5000A,
	PICO6000A,
	PICOPSOSPA
};

/**
	@brief The scopehal-pico-bridge server (ps6000d), running inside ngscopeclient

	Control plane commands go in through ProcessLine() and replies come out through PopReply(). Waveform data comes
	out of GetDataPipe() in the bridge's binary format, and ACKs go back in through GetAckPipe().

	In ps6000d all this state was global, one instrument per process. Here it's one object per instrument: the
	globals g_foo are members m_foo, and the free functions are member functions.

	@ingroup picobridge
 */
class PicoSCPIServer : public BridgeSCPIServer
{
public:
	PicoSCPIServer();
	virtual ~PicoSCPIServer();

	//not copyable or assignable
	PicoSCPIServer(const PicoSCPIServer&) =delete;
	PicoSCPIServer& operator=(const PicoSCPIServer&) =delete;

	PICO_STATUS Open(unsigned int series, const std::string& serial);
	void StartClient();
	void StopClient();
	void Shutdown();

	static std::vector<std::pair<std::string, std::string>> EnumerateUnits();

	///@brief Waveform data from the bridge to the driver
	PicoDataPipe& GetDataPipe()
	{ return m_dataPipe; }

	///@brief ACKs from the driver to the bridge
	PicoDataPipe& GetAckPipe()
	{ return m_ackPipe; }

	std::string GetMake() override;
	std::string GetModel() override;
	std::string GetSerial() override;
	std::string GetFirmwareVersion() override;

protected:
	size_t GetAnalogChannelCount() override;
	std::vector<size_t> GetSampleRates() override;
	std::vector<size_t> GetSampleDepths() override;

	bool OnCommand(
		const std::string& line,
		const std::string& subject,
		const std::string& cmd,
		const std::vector<std::string>& args) override;

	bool OnQuery(
		const std::string& line,
		const std::string& subject,
		const std::string& cmd) override;

	bool GetChannelID(const std::string& subject, size_t& id_out) override;
	ChannelType GetChannelType(size_t channel) override;

	void ReconfigAWG();

	//Command methods
	void AcquisitionStart(bool oneShot = false) override;
	void AcquisitionForceTrigger() override;
	void AcquisitionStop() override;
	void SetChannelEnabled(size_t chIndex, bool enabled) override;
	void SetAnalogCoupling(size_t chIndex, const std::string& coupling) override;
	void SetAnalogRange(size_t chIndex, double range_V) override;
	void SetAnalogOffset(size_t chIndex, double offset_V) override;
	void SetDigitalThreshold(size_t chIndex, double threshold_V) override;
	void SetDigitalHysteresis(size_t chIndex, double hysteresis) override;
	virtual void SetChannelBandwidthLimiter(size_t i, unsigned int limit_mhz);

	void SetSampleRate(uint64_t rate_hz) override;
	void SetSampleDepth(uint64_t depth) override;
	void SetTriggerDelay(uint64_t delay_fs) override;
	void SetTriggerSource(size_t chIndex) override;
	void SetTriggerLevel(double level_V) override;
	void SetTriggerTypeEdge() override;
	void SetEdgeTriggerEdge(const std::string& edge) override;
	bool IsTriggerArmed() override;

	//Formerly free functions (ps6000d.h, main.cpp, WaveformServerThread.cpp)
	void Stop();
	void StartCapture(bool stopFirst, bool force = false);
	PICO_STATUS StartInternal();
	void UpdateTrigger(bool force = false);
	void UpdateChannel(size_t chan);
	bool EnableMsoPod(size_t npod);
	void GenerateSquareWave(int16_t* &waveform, size_t bufferSize, double dutyCycle, int16_t amplitude = 32767);
	PICO_INFO Open2000(int8_t* serial);
	PICO_INFO Open3000(int8_t* serial);
	PICO_INFO Open4000(int8_t* serial);
	PICO_INFO Open5000(int8_t* serial);
	PICO_INFO Open6000(int8_t* serial);
	void CloseUnit();
	void WaveformServerThread();
	void CheckForACKs(bool wait);
	float InterpolateTriggerTime(int16_t* buf);

protected:
	const PicoApi& m_api;

	///@brief True while a client is connected (between StartClient() and StopClient())
	bool m_clientStarted;

	PicoDataPipe m_dataPipe;
	PicoDataPipe m_ackPipe;
	std::thread m_waveformThread;

	//main.cpp
	PICO_STATUS (*m_picoGetUnitInfo) (int16_t, int8_t *, int16_t, int16_t *, PICO_INFO) = nullptr;
	std::string m_model;		//model number, used to discern features
	std::string m_serial;
	std::string m_fwver;
	size_t m_series = 0;		//single 1st digit: 2,3,4,5,6
	PicoScopeType m_pico_type = PICO6000A;
	int16_t m_hScope = 0;
	size_t m_numChannels = 0;
	bool m_limitChannels = false;

	//WaveformServerThread.cpp
	std::atomic<bool> m_waveformThreadQuit;
	std::vector<PICO_CHANNEL> m_channelIDs;
	uint32_t m_lastTxSeq = 0;
	uint32_t m_lastRxAck = 0;

	//PicoSCPIServer.cpp

	//Channel state
	std::map<size_t, bool> m_channelOn;
	std::map<size_t, PICO_COUPLING> m_coupling;
	std::map<size_t, PICO_CONNECT_PROBE_RANGE> m_range;
	std::map<size_t, enPS2000ARange> m_range_2000a;
	std::map<size_t, enPS3000ARange> m_range_3000a;
	std::map<size_t, enPS4000ARange> m_range_4000a;
	std::map<size_t, enPS5000ARange> m_range_5000a;
	std::map<size_t, PICO_PROBE_RANGE_INFO> m_range_psospa;
	std::map<size_t, double> m_roundedRange;
	std::map<size_t, double> m_range_3000e;
	std::map<size_t, double> m_offset;
	std::map<size_t, double> m_msoPodThresholdVoltage;
	std::map<size_t, PICO_BANDWIDTH_LIMITER> m_bandwidth;	//6000A,PSOSPA
	std::map<size_t, enPS3000ABandwidthLimiter> m_bandwidth_3000a;
	std::map<size_t, enPS4000ABandwidthLimiter> m_bandwidth_4000a;
	std::map<size_t, enPS5000ABandwidthLimiter> m_bandwidth_5000a;
	size_t m_memDepth = 1000000;
	size_t m_scaleValue = 32512;
	size_t m_adcBits = 8;
	int64_t m_sampleInterval = 0;	//in fs

	//Copy of state at timestamp of last arm event
	std::map<size_t, bool> m_channelOnDuringArm;
	int64_t m_sampleIntervalDuringArm = 0;
	size_t m_captureMemDepth = 0;
	std::map<size_t, double> m_offsetDuringArm;

	uint32_t m_timebase = 0;
	uint32_t m_sampleRate = 0;

	bool m_triggerArmed = false;
	bool m_triggerOneShot = false;
	bool m_memDepthChanged = false;

	//Trigger state (for now, only simple single-channel trigger supported)
	int64_t m_triggerDelay = 0;
	PICO_THRESHOLD_DIRECTION m_triggerDirection = PICO_RISING;
	float m_triggerVoltage = 0;
	size_t m_triggerChannel = 0;
	size_t m_triggerSampleIndex = 0;

	//Thresholds for MSO pods
	size_t m_numDigitalPods = 2;
	int16_t m_msoPodThreshold[2][8] = { {0}, {0} };
	PICO_DIGITAL_PORT_HYSTERESIS m_msoHysteresis[2] = {PICO_NORMAL_100MV, PICO_NORMAL_100MV};
	bool m_msoPodEnabled[2] = {false};
	bool m_msoPodEnabledDuringArm[2] = {false};

	bool m_lastTriggerWasForced = false;

	std::mutex m_mutex;

	//AWG config
	float m_awgRange = 0;
	float m_awgOffset = 0;
	bool m_awgOn = false;
	double m_awgFreq = 1000;
	int32_t m_awgBufferSize = 8192;
	PS2000A_EXTRA_OPERATIONS m_awgPS2000AOperation = PS2000A_ES_OFF;
	PS2000A_WAVE_TYPE m_awgPS2000AWaveType = PS2000A_SINE;
	PS3000A_EXTRA_OPERATIONS m_awgPS3000AOperation = PS3000A_ES_OFF;    // Noise and PRBS generation is not a WaveType
	PS3000A_WAVE_TYPE m_awgPS3000AWaveType = PS3000A_SINE;              // Waveform must be set in ReconfigAWG(), holds the WaveType;
	PS4000A_EXTRA_OPERATIONS m_awgPS4000AOperation = PS4000A_ES_OFF;
	PS4000A_WAVE_TYPE m_awgPS4000AWaveType = PS4000A_SINE;
	PS5000A_EXTRA_OPERATIONS m_awgPS5000AOperation = PS5000A_ES_OFF;
	PS5000A_WAVE_TYPE m_awgPS5000AWaveType = PS5000A_SINE;

	int16_t* m_arbitraryWaveform = nullptr;
};

}

#endif
