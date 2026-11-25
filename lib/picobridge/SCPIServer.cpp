/***********************************************************************************************************************
*                                                                                                                      *
* scpi-server-tools                                                                                                    *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of SCPIServer
	@ingroup picobridge
 */

#ifdef HAS_PICO

#include "picobridge.h"
#include "SCPIServer.h"

using namespace std;

namespace picobridge
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SCPIServer::SCPIServer()
{
}

SCPIServer::~SCPIServer()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reply queue

/**
	@brief Queues a SCPI reply for the client

	@param cmd	Reply to send
 */
bool SCPIServer::SendReply(const string& cmd)
{
	m_replies.push_back(cmd);
	return true;
}

/**
	@brief Gets the oldest reply not yet collected

	@return False if there are no replies waiting
 */
bool SCPIServer::PopReply(string& reply)
{
	if(m_replies.empty())
		return false;
	reply = m_replies.front();
	m_replies.pop_front();
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SCPI command parsing

/**
	@brief Crack an inbound SCPI query into its component parts

	@param line		The inbound SCPI command
	@param subject	The object that the command operates on (for example, outputs C2 if line is C2:OFFS)
	@param cmd		The main text of the command (for example, outputs OFFS if line is C2:OFFS)
	@param query	Outputs true if the command is a query, false if not
	@param args		List of arguments after the main command
 */
void SCPIServer::ParseLine(
	const string& line,
	string& subject,
	string& cmd,
	bool& query,
	vector<string>& args)
{
	//Reset fields
	query = false;
	subject = "";
	cmd = "";
	args.clear();

	string tmp;
	bool reading_cmd = true;
	for(size_t i=0; i<line.length(); i++)
	{
		//If there's no colon in the command, the first block is the command.
		//If there is one, the first block is the subject and the second is the command.
		//If more than one, treat it as freeform text in the command.
		if( (line[i] == ':') && subject.empty() )
		{
			subject = tmp;
			tmp = "";
			continue;
		}

		//Detect queries
		if(line[i] == '?')
		{
			query = true;
			continue;
		}

		//Comma delimits arguments, space delimits command-to-args
		if(!(isspace(line[i]) && cmd.empty()) && line[i] != ',')
		{
			tmp += line[i];
			continue;
		}

		//merge multiple delimiters into one delimiter
		if(tmp == "")
			continue;

		//Save command or argument
		if(reading_cmd)
			cmd = tmp;
		else
			args.push_back(tmp);

		reading_cmd = false;
		tmp = "";
	}

	//Stuff left over at the end? Figure out which field it belongs in
	if(tmp != "")
	{
		if(cmd != "")
			args.push_back(tmp);
		else
			cmd = tmp;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command processing

/**
	@brief Processes one command (the socket server's MainLoop body)

	The line must not contain the newline or semicolon terminator.
 */
void SCPIServer::ProcessLine(const string& line)
{
	string cmd;
	bool query;
	string subject;
	vector<string> args;

	LogTrace("%s\n", line.c_str());
	ParseLine(line, subject, cmd, query, args);

	//Process the command
	if(query)
		OnQuery(line, subject, cmd);
	else if(cmd == "EXIT")
		return;
	else
		OnCommand(line, subject, cmd, args);
}

}

#endif
