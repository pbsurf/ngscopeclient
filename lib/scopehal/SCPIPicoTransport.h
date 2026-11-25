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
	@brief Declaration of SCPIPicoTransport
	@ingroup transports
 */

#ifdef HAS_PICO

#ifndef SCPIPicoTransport_h
#define SCPIPicoTransport_h

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace picobridge
{
	class PicoSCPIServer;
}

/**
	@brief Transport for PicoScopes, running scopehal-pico-bridge inside ngscopeclient

	This speaks the same protocol as the twinlan transport to a separate scopehal-pico-bridge (ps6000d) process,
	but the bridge runs in process (lib/scopehal/picobridge) and talks to the Pico SDK directly. Commands are queued
	for a thread that runs the bridge's command handler (as ps6000d's socket loop did, so SendCommand() doesn't wait
	for the SDK), and waveform data comes through an in-memory pipe instead of a socket.

	The SDK libraries are loaded the first time this transport is used or enumerated, so ngscopeclient still runs on
	machines without the Pico SDK.

	The path is the serial number of the instrument to open (for example "JR123/0045"), or empty to open the first
	one found.

	@ingroup transports
 */
class SCPIPicoTransport : public SCPITransport
{
public:
	SCPIPicoTransport(const std::string& args);
	virtual ~SCPIPicoTransport();

	//not copyable or assignable
	SCPIPicoTransport(const SCPIPicoTransport&) =delete;
	SCPIPicoTransport& operator=(const SCPIPicoTransport&) =delete;

	virtual std::string GetConnectionString() override;
	static std::string GetTransportName();

	virtual bool SendCommand(const std::string& cmd) override;
	virtual std::string ReadReply(bool endOnSemicolon = true, std::function<void(float)> progress = nullptr) override;
	virtual size_t ReadRawData(size_t len, unsigned char* buf, std::function<void(float)> progress = nullptr) override;
	virtual void SendRawData(size_t len, const unsigned char* buf) override;

	virtual bool IsCommandBatchingSupported() override;
	virtual bool IsConnected() override;

	virtual void FlushRXBuffer() override;

	bool IsDataAvailable();

	static std::vector<TransportEndpoint> EnumTransportEndpoints();

	TRANSPORT_INITPROC(SCPIPicoTransport)

protected:
	void CommandThread();

	std::string m_serial;

	///@brief The bridge (null if we failed to connect)
	std::unique_ptr<picobridge::PicoSCPIServer> m_server;

	///@brief Runs the commands in m_commands
	std::thread m_commandThread;

	///@brief Protects m_commands, m_replies, m_quit
	std::mutex m_mutex;

	///@brief Signalled when a command or reply is queued, or on m_quit
	std::condition_variable m_cond;

	///@brief Commands waiting for the command thread
	std::deque<std::string> m_commands;

	///@brief Replies waiting for ReadReply()
	std::deque<std::string> m_replies;

	///@brief Set to stop the command thread
	bool m_quit;
};

#endif

#endif
