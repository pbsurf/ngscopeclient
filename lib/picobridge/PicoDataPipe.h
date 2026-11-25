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
	@brief Declaration of PicoDataPipe
	@ingroup picobridge
 */

#ifndef PicoDataPipe_h
#define PicoDataPipe_h

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace picobridge
{

/**
	@brief One way in-process byte stream, replacing the bridge's waveform socket

	Small writes are copied and return at once. Large writes (waveform data) are not copied: Write() waits until the
	reader has copied the data out of the writer's buffer, so each waveform is copied once, straight into the
	driver's buffer.

	@ingroup picobridge
 */
class PicoDataPipe
{
public:
	PicoDataPipe();

	//not copyable or assignable
	PicoDataPipe(const PicoDataPipe&) =delete;
	PicoDataPipe& operator=(const PicoDataPipe&) =delete;

	bool Write(const void* buf, size_t len);
	size_t Read(void* buf, size_t len, unsigned int timeoutMs);
	size_t ReadSome(void* buf, size_t maxlen, unsigned int timeoutMs);
	size_t BytesAvailable();
	bool WaitForData(unsigned int timeoutMs);

	void Close();
	void Reopen();

	///@brief Writes larger than this wait for the reader instead of being copied
	static const size_t COPY_LIMIT = 65536;

protected:
	struct Chunk
	{
		///@brief Our copy of the data (small writes only)
		std::vector<uint8_t> copy;

		///@brief Start of the data (in copy, or in the writer's buffer)
		const uint8_t* data;

		size_t len;

		///@brief Bytes already read
		size_t offset;

		///@brief Sequence number, to tell a waiting writer its chunk was read
		uint64_t id;
	};

	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::deque<Chunk> m_chunks;

	///@brief True if Close() was called (writes fail, reads only get data already queued)
	bool m_closed;

	///@brief Id the next chunk gets
	uint64_t m_nextId;

	///@brief Id of the last chunk read completely
	uint64_t m_lastConsumedId;
};

}

#endif
