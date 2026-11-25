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
	@brief Implementation of PicoDataPipe
	@ingroup picobridge
 */

#ifdef HAS_PICO

#include "picobridge.h"
#include "PicoDataPipe.h"

#include <cstring>

using namespace std;

namespace picobridge
{

PicoDataPipe::PicoDataPipe()
	: m_closed(false)
	, m_nextId(1)
	, m_lastConsumedId(0)
{
}

/**
	@brief Writes data to the pipe

	@return False if the pipe was closed
 */
bool PicoDataPipe::Write(const void* buf, size_t len)
{
	if(len == 0)
		return true;

	unique_lock<mutex> lock(m_mutex);
	if(m_closed)
		return false;

	Chunk c;
	c.len = len;
	c.offset = 0;
	c.id = m_nextId ++;
	bool wait = (len > COPY_LIMIT);
	if(wait)
		c.data = reinterpret_cast<const uint8_t*>(buf);
	else
	{
		c.copy.assign(reinterpret_cast<const uint8_t*>(buf), reinterpret_cast<const uint8_t*>(buf) + len);
		c.data = c.copy.data();
	}
	uint64_t id = c.id;
	m_chunks.push_back(std::move(c));
	m_cond.notify_all();

	if(!wait)
		return true;

	//The reader copies straight out of our buffer, so wait for it to finish
	m_cond.wait(lock, [&]{ return m_closed || (m_lastConsumedId >= id); });

	//If we were closed before the reader got to it, it must not touch the buffer after we return
	if(m_lastConsumedId < id)
	{
		m_chunks.clear();
		return false;
	}
	return true;
}

/**
	@brief Reads exactly len bytes, waiting up to timeoutMs for each chunk of them to arrive

	@return len, or 0 if the pipe was closed or the wait timed out (the stream is then out of sync)
 */
size_t PicoDataPipe::Read(void* buf, size_t len, unsigned int timeoutMs)
{
	auto out = reinterpret_cast<uint8_t*>(buf);
	size_t done = 0;

	unique_lock<mutex> lock(m_mutex);
	while(done < len)
	{
		if(m_chunks.empty())
		{
			if(m_closed)
				return 0;
			if(!m_cond.wait_for(lock, chrono::milliseconds(timeoutMs), [&]{ return m_closed || !m_chunks.empty(); }))
				return 0;
			continue;
		}

		auto& c = m_chunks.front();
		size_t n = min(len - done, c.len - c.offset);
		memcpy(out + done, c.data + c.offset, n);
		done += n;
		c.offset += n;

		if(c.offset == c.len)
		{
			m_lastConsumedId = c.id;
			m_chunks.pop_front();
			m_cond.notify_all();
		}
	}
	return len;
}

/**
	@brief Reads whatever is waiting, up to maxlen bytes, waiting up to timeoutMs for something to arrive

	@return Number of bytes read (0 on timeout, or if the pipe was closed and is empty)
 */
size_t PicoDataPipe::ReadSome(void* buf, size_t maxlen, unsigned int timeoutMs)
{
	auto out = reinterpret_cast<uint8_t*>(buf);
	size_t done = 0;

	unique_lock<mutex> lock(m_mutex);
	m_cond.wait_for(lock, chrono::milliseconds(timeoutMs), [&]{ return m_closed || !m_chunks.empty(); });

	while( (done < maxlen) && !m_chunks.empty() )
	{
		auto& c = m_chunks.front();
		size_t n = min(maxlen - done, c.len - c.offset);
		memcpy(out + done, c.data + c.offset, n);
		done += n;
		c.offset += n;

		if(c.offset == c.len)
		{
			m_lastConsumedId = c.id;
			m_chunks.pop_front();
			m_cond.notify_all();
		}
	}
	return done;
}

///@brief Returns the number of bytes waiting to be read
size_t PicoDataPipe::BytesAvailable()
{
	lock_guard<mutex> lock(m_mutex);
	size_t n = 0;
	for(auto& c : m_chunks)
		n += c.len - c.offset;
	return n;
}

/**
	@brief Waits up to timeoutMs for data to arrive

	@return True if there's data to read
 */
bool PicoDataPipe::WaitForData(unsigned int timeoutMs)
{
	unique_lock<mutex> lock(m_mutex);
	m_cond.wait_for(lock, chrono::milliseconds(timeoutMs), [&]{ return m_closed || !m_chunks.empty(); });
	return !m_chunks.empty();
}

///@brief Closes the pipe, waking anything waiting on it
void PicoDataPipe::Close()
{
	lock_guard<mutex> lock(m_mutex);
	m_closed = true;
	m_cond.notify_all();
}

///@brief Throws away anything queued and opens the pipe again after Close()
void PicoDataPipe::Reopen()
{
	lock_guard<mutex> lock(m_mutex);
	m_chunks.clear();
	m_closed = false;
}

}

#endif
