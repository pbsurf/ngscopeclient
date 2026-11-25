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
	@brief Program entry point for the standalone ps6000d socket server

	This is upstream scopehal-pico-bridge's main.cpp, cut down to the sockets: the bridge itself is lib/picobridge,
	shared with ngscopeclient's built in pico transport. The protocol is unchanged, so ngscopeclient (or anything else
	that speaks it) connects with the twinlan transport as before.
 */

#include <xptools/Socket.h>	//first: on Windows it has to come before windows.h
#include <picobridge/picobridge.h>
#include <picobridge/PicoSCPIServer.h>

#include <atomic>
#include <signal.h>

using namespace std;
using namespace picobridge;

void help();
void ScpiLoop(Socket& client, PicoSCPIServer& server);
bool RecvCommand(Socket& client, string& str);
void DataForwardThread(Socket* listener, PicoSCPIServer* server, atomic<bool>* quit);

void help()
{
	fprintf(stderr,
			"ps6000d [general options] [logger options]\n"
			"\n"
			"  [general options]:\n"
			"    --help                        : this message...\n"
			"    --series <number>             : specifies the model series to look for (2000, 3000, 4000, 5000, 6000)\n"
			"    --serial <serial>             : specifies the serial number of the instrument to open\n"
			"    --scpi-port port              : specifies the SCPI control plane port (default 5025)\n"
			"    --waveform-port port          : specifies the binary waveform data port (default 5026)\n"
			"\n"
			"  [logger options]:\n"
			"    levels: ERROR, WARNING, NOTICE, VERBOSE, DEBUG\n"
			"    --quiet|-q                    : reduce logging level by one step\n"
			"    --verbose                     : set logging level to VERBOSE\n"
			"    --debug                       : set logging level to DEBUG\n"
			"    --trace <classname>|          : name of class with tracing messages. (Only relevant when logging level is DEBUG.)\n"
			"            <classname::function>\n"
			"    --logfile|-l <filename>       : output log messages to file\n"
			"    --logfile-lines|-L <filename> : output log messages to file, with line buffering\n"
			"    --stdout-only                 : writes errors/warnings to stdout instead of stderr\n"
		   );
}

static PicoSCPIServer* g_server = nullptr;

#ifdef _WIN32
BOOL WINAPI OnQuit(DWORD signal);
#else
void OnQuit(int signal);
#endif

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;
	unsigned int series = 0;
	string serial;

	//Parse command-line arguments
	uint16_t scpi_port = 5025;
	uint16_t waveform_port = 5026;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if(s == "--help")
		{
			help();
			return 0;
		}

		else if(s == "--series")
		{
			if(i+1 < argc)
			{
				string tmp(argv[++i]);
				series = tmp[0] - '0';
				if( !( (series==2) || (series==3) || (series==4) || (series==5) || (series==6) ) )
					series = 0;
			}
		}

		else if(s == "--serial")
		{
			if(i+1 < argc)
				serial = argv[++i];
		}

		else if(s == "--scpi-port")
		{
			if(i+1 < argc)
				scpi_port = atoi(argv[++i]);
		}

		else if(s == "--waveform-port")
		{
			if(i+1 < argc)
				waveform_port = atoi(argv[++i]);
		}

		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Load the SDK libraries
	if(!GetPicoApi().IsAnyLoaded())
	{
		LogError("No Pico SDK libraries (ps6000a.dll etc) found. Install the Pico SDK, or copy its libraries here.\n");
		return 1;
	}

	//Open the instrument
	PicoSCPIServer server;
	if(server.Open(series, serial) != PICO_OK)
		return 1;
	LogNotice("Listening on ports %i, %i\n", scpi_port, waveform_port);

	//Set up signal handlers
	g_server = &server;
#ifdef _WIN32
	SetConsoleCtrlHandler(OnQuit, TRUE);
#else
	signal(SIGINT, OnQuit);
	signal(SIGPIPE, SIG_IGN);
#endif

	//Configure the data plane socket
	Socket dataSocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	dataSocket.Bind(waveform_port);
	dataSocket.Listen();

	//Launch the control plane socket server
	Socket scpiSocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	scpiSocket.Bind(scpi_port);
	scpiSocket.Listen();

	while(true)
	{
		Socket scpiClient = scpiSocket.Accept();
		if(!scpiClient.IsValid())
			break;
		LogVerbose("Client connected to SCPI socket\n");
		if(!scpiClient.DisableNagle())
			LogWarning("Failed to disable Nagle on socket, performance may be poor\n");

		//Launch the bridge's waveform thread, and our thread to pass its data to the data-plane socket
		server.StartClient();
		atomic<bool> quit(false);
		thread dataThread(DataForwardThread, &dataSocket, &server, &quit);

		//Process connections on the socket
		ScpiLoop(scpiClient, server);

		server.StopClient();
		quit = true;
		dataThread.join();
	}

	//Done (the destructor closes the instrument)
	g_server = nullptr;
	return 0;
}

/**
	@brief Runs commands from the client until it disconnects or sends EXIT (formerly SCPIServer::MainLoop())
 */
void ScpiLoop(Socket& client, PicoSCPIServer& server)
{
	string line;
	while(true)
	{
		//Get the inbound command
		if(!RecvCommand(client, line))
			break;
		if(Trim(line) == "EXIT")
			break;

		server.ProcessLine(line);

		//Send any replies
		string reply;
		while(server.PopReply(reply))
		{
			reply += "\n";
			if(!client.SendLooped((const unsigned char*)reply.c_str(), reply.length()))
				return;
		}
	}
}

/**
	@brief Reads a SCPI command (terminated by newline or semicolon)

	@param str	Output command
 */
bool RecvCommand(Socket& client, string& str)
{
	unsigned char tmp = ' ';
	str = "";
	while(true)
	{
		if(!client.RecvLooped(&tmp, 1))
			return false;

		if( (tmp == '\n') || ( (tmp == ';') ) )
			break;
		else
			str += tmp;
	}

	return true;
}

/**
	@brief Accepts the data-plane connection, then passes waveform data to it and ACKs from it

	The bridge's waveform thread used to do this itself, with the socket. Now it writes to a pipe.
 */
void DataForwardThread(Socket* listener, PicoSCPIServer* server, atomic<bool>* quit)
{
#ifdef __linux__
	pthread_setname_np(pthread_self(), "DataForward");
#endif

	Socket client = listener->Accept();
	if(!client.IsValid())
		return;
	LogVerbose("Client connected to data plane socket\n");
	if(!client.DisableNagle())
		LogWarning("Failed to disable Nagle on socket, performance may be poor\n");

	auto& data = server->GetDataPipe();
	auto& acks = server->GetAckPipe();
	vector<uint8_t> buf(1024 * 1024);
	while(!*quit)
	{
		size_t n = data.ReadSome(buf.data(), buf.size(), 1);
		if(n && !client.SendLooped(buf.data(), n))
			break;

		while(client.GetRxBytesAvailable() >= 4)
		{
			uint8_t ack[4];
			if(!client.RecvLooped(ack, sizeof(ack)))
				break;
			acks.Write(ack, sizeof(ack));
		}
	}

	LogDebug("Client disconnected from data plane socket\n");
}

#ifdef _WIN32
BOOL WINAPI OnQuit(DWORD signal)
{
	(void)signal;
#else
void OnQuit(int /*signal*/)
{
#endif
	if(g_server)
		g_server->Shutdown();
	exit(0);
}
