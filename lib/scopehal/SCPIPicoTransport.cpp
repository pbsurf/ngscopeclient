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
	@brief Implementation of SCPIPicoTransport
	@ingroup transports
 */

#ifdef HAS_PICO

#include "scopehal.h"
#include "SCPIPicoTransport.h"
#include <picobridge/PicoSCPIServer.h>

using namespace std;
using namespace picobridge;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SCPIPicoTransport::SCPIPicoTransport(const string& args)
	: m_serial(Trim(args))
	, m_quit(false)
{
	if(!GetPicoApi().IsAnyLoaded())
	{
		LogError("Can't connect to the PicoScope: no Pico SDK libraries (ps6000a.dll etc) found\n");
		return;
	}

	if(m_serial.empty())
		LogDebug("Opening the first PicoScope found\n");
	else
		LogDebug("Opening PicoScope %s\n", m_serial.c_str());

	m_server = make_unique<PicoSCPIServer>();
	if(m_server->Open(0, m_serial) != PICO_OK)
	{
		m_server = nullptr;
		return;
	}
	m_server->StartClient();

	m_commandThread = thread(&SCPIPicoTransport::CommandThread, this);
}

SCPIPicoTransport::~SCPIPicoTransport()
{
	//Let the command thread finish what's queued, then stop it
	{
		lock_guard<mutex> lock(m_mutex);
		m_quit = true;
		m_cond.notify_all();
	}
	if(m_commandThread.joinable())
		m_commandThread.join();

	//Stops the waveform thread and closes the instrument
	m_server = nullptr;
}

/**
	@brief Runs commands for the bridge, one at a time (ps6000d's SCPIServer::MainLoop())
 */
void SCPIPicoTransport::CommandThread()
{
	#ifdef __linux__
	pthread_setname_np(pthread_self(), "PicoCommand");
	#endif

	while(true)
	{
		string line;
		{
			unique_lock<mutex> lock(m_mutex);
			m_cond.wait(lock, [&]{ return m_quit || !m_commands.empty(); });
			if(m_commands.empty())
				return;
			line = m_commands.front();
			m_commands.pop_front();
		}

		m_server->ProcessLine(line);

		//Pass on any reply
		string reply;
		while(m_server->PopReply(reply))
		{
			lock_guard<mutex> lock(m_mutex);
			m_replies.push_back(reply);
			m_cond.notify_all();
		}
	}
}

bool SCPIPicoTransport::IsConnected()
{
	return m_server != nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual transport code

string SCPIPicoTransport::GetTransportName()
{
	return "pico";
}

string SCPIPicoTransport::GetConnectionString()
{
	return m_serial;
}

/**
	@brief Lists the PicoScopes that are connected and not already open

	The first call loads the Pico SDK libraries.
 */
vector<TransportEndpoint> SCPIPicoTransport::EnumTransportEndpoints()
{
	vector<TransportEndpoint> result;
	for(auto& it : PicoSCPIServer::EnumerateUnits())
	{
		TransportEndpoint endpoint;
		endpoint.path = it.first;
		endpoint.description = it.second;
		result.push_back(endpoint);
	}
	return result;
}

/**
	@brief Queues one or more commands (separated by semicolons or newlines, as on the bridge's socket)
 */
bool SCPIPicoTransport::SendCommand(const string& cmd)
{
	if(!m_server)
		return false;

	lock_guard<mutex> lock(m_mutex);

	string line;
	for(auto c : cmd)
	{
		if( (c == '\n') || (c == ';') )
		{
			if(!line.empty())
				m_commands.push_back(line);
			line = "";
		}
		else
			line += c;
	}
	if(!line.empty())
		m_commands.push_back(line);
	m_cond.notify_all();

	return true;
}

/**
	@brief Waits for the reply to a query
 */
string SCPIPicoTransport::ReadReply(bool /*endOnSemicolon*/, [[maybe_unused]] function<void(float)> progress)
{
	if(!m_server)
		return "";

	unique_lock<mutex> lock(m_mutex);
	if(!m_cond.wait_for(lock, chrono::seconds(5), [&]{ return !m_replies.empty(); }))
	{
		LogWarning("SCPIPicoTransport: timed out waiting for a reply from the bridge\n");
		return "";
	}

	string reply = m_replies.front();
	m_replies.pop_front();
	return reply;
}

void SCPIPicoTransport::FlushRXBuffer()
{
	lock_guard<mutex> lock(m_mutex);
	m_replies.clear();
}

///@brief Sends an ACK (or other data) to the bridge's waveform thread
void SCPIPicoTransport::SendRawData(size_t len, const unsigned char* buf)
{
	if(m_server)
		m_server->GetAckPipe().Write(buf, len);
}

///@brief Reads waveform data from the bridge's waveform thread
size_t SCPIPicoTransport::ReadRawData(size_t len, unsigned char* buf, std::function<void(float)> /*progress*/)
{
	if(!m_server)
		return 0;
	return m_server->GetDataPipe().Read(buf, len, 5000);
}

///@brief Returns true if there's waveform data waiting to be read
bool SCPIPicoTransport::IsDataAvailable()
{
	if(!m_server)
		return false;
	return m_server->GetDataPipe().BytesAvailable() > 0;
}

bool SCPIPicoTransport::IsCommandBatchingSupported()
{
	return true;
}

#endif
