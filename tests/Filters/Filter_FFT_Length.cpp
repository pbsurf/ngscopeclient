/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
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
	@brief Unit test for the FFT filter's FFT Length setting (averaging shorter FFTs)
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif


#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/FilterGraphExecutor.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Filters.h"

using namespace std;

/**
	@brief Fills a waveform with a sine of amplitude amp at frequency hz, sampled at 1 Gsps
 */
static void MakeTone(UniformAnalogWaveform& wfm, size_t depth, double hz, double amp)
{
	wfm.Resize(depth);
	wfm.PrepareForCpuAccess();
	for(size_t n=0; n<depth; n++)
		wfm.m_samples[n] = amp * sin(2 * M_PI * hz * n * 1e-9);
	wfm.MarkModifiedFromCpu();
	wfm.m_revision ++;
}

/**
	@brief Finds the largest value in a spectrum and returns the index of it
 */
static size_t FindPeakBin(UniformAnalogWaveform* wfm)
{
	wfm->PrepareForCpuAccess();
	size_t best = 0;
	for(size_t i=0; i<wfm->size(); i++)
	{
		if(wfm->m_samples[i] > wfm->m_samples[best])
			best = i;
	}
	return best;
}

/**
	@brief Returns the total power of the bins of a spectrum, in dBm
 */
static double TotalPower(UniformAnalogWaveform* wfm)
{
	wfm->PrepareForCpuAccess();
	double mw = 0;
	for(size_t i=0; i<wfm->size(); i++)
		mw += pow(10, wfm->m_samples[i] / 10);
	return 10 * log10(mw);
}

TEST_CASE("Filter_FFT_Length")
{
	auto filter = dynamic_cast<FFTFilter*>(Filter::CreateFilter("FFT", "#ffffff"));
	REQUIRE(filter != nullptr);
	FilterReferencer ref(filter);

	FilterGraphExecutor exec;
	set<FlowGraphNode*> nodes;
	nodes.emplace(filter);

	const size_t depth = 65536;
	const double sampleRate = 1e9;
	UniformAnalogWaveform ua;
	ua.m_timescale = 1000000;	//1 Gsps
	ua.m_triggerPhase = 0;

	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);
	filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));

	//A sine of amplitude A is A^2/2 into 50 ohms: 10 dBm for A = 1
	const double amp = 1;
	const double expectedDbm = 10 * log10(amp * amp / 2 / 50) + 30;

	//On bin 100 of a 1024 point FFT, so also on a bin of the whole input (6400)
	const size_t fftlen = 1024;
	const double toneHz = 100 * sampleRate / fftlen;

	auto run = [&]() -> UniformAnalogWaveform*
	{
		exec.RunBlocking(nodes);
		auto out = dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0));
		REQUIRE(out != nullptr);
		return out;
	};

	//Checks the size, bin width, and the tone's bin, frequency and level
	auto checkTone = [&](size_t npoints)
	{
		auto out = run();
		REQUIRE(out->size() == npoints/2 + 1);

		//Bins are the sample rate / FFT size apart (not / number of bins)
		REQUIRE(out->m_timescale == llround(sampleRate / npoints * 1e6));

		size_t peak = FindPeakBin(out);
		REQUIRE(static_cast<long long>(peak) == llround(toneHz * npoints / sampleRate));
		double peakHz = (out->m_triggerPhase + static_cast<int64_t>(peak) * out->m_timescale) / 1e6;
		REQUIRE(fabs(peakHz - toneHz) < 1);
		REQUIRE(fabs(out->m_samples[peak] - expectedDbm) < 0.3);
	};

	MakeTone(ua, depth, toneHz, amp);
	for(auto window : { FFTFilter::WINDOW_RECTANGULAR, FFTFilter::WINDOW_HANN, FFTFilter::WINDOW_HAMMING,
		FFTFilter::WINDOW_BLACKMAN_HARRIS })
	{
		INFO("window " << window);
		filter->SetWindowFunction(window);

		//Whole input
		filter->GetParameter("FFT Length").SetIntVal(0);
		checkTone(depth);

		//127 segments of 1024 points, overlapping by half. Same tone level, 64x wider bins.
		filter->GetParameter("FFT Length").SetIntVal(fftlen);
		checkTone(fftlen);
	}

	//Same FFT length with a different input length: the segment count changes, so the batched plan must too
	MakeTone(ua, depth/2, toneHz, amp);
	checkTone(fftlen);
	MakeTone(ua, depth, toneHz, amp);
	checkTone(fftlen);

	//An FFT length longer than the input is the whole input
	filter->GetParameter("FFT Length").SetIntVal(131072);
	checkTone(depth);

	//With a rectangular window the total power of the bins is the input's power (Parseval), however long the FFT.
	//Noise is spread over all the bins, so this tests the scaling of the average, not just of one bin.
	//(The one sided spectrum doubles the power of every bin, including DC and Nyquist, which is 2/N too much.)
	filter->SetWindowFunction(FFTFilter::WINDOW_RECTANGULAR);
	FillRandomWaveform(&ua, depth);
	filter->GetParameter("FFT Length").SetIntVal(0);
	double whole = TotalPower(run());
	filter->GetParameter("FFT Length").SetIntVal(fftlen);
	double averaged = TotalPower(run());
	INFO("whole " << whole << " dBm, averaged " << averaged << " dBm");
	REQUIRE(fabs(averaged - whole) < 0.2);

	//RBW: the window's equivalent noise bandwidth times the bin width. For white noise, the mean power per bin with
	//a window, relative to the rectangular window, is the ENBW, given the coherent gain scaling of the tone level.
	//(The averaged FFT keeps the noise estimate steady.)
	auto meanPower = [&](FFTFilter::WindowFunction window)
	{
		filter->SetWindowFunction(window);
		auto out = run();
		double mw = pow(10, TotalPower(out) / 10) / out->size();
		INFO("window " << window);
		REQUIRE(fabs(filter->GetRBW() / (out->m_timescale * FFTFilter::GetWindowENBW(window)) - 1) < 1e-9);
		return mw;
	};
	double rect = meanPower(FFTFilter::WINDOW_RECTANGULAR);
	for(auto window : { FFTFilter::WINDOW_HANN, FFTFilter::WINDOW_HAMMING, FFTFilter::WINDOW_BLACKMAN_HARRIS })
	{
		double ratio = meanPower(window) / rect;
		INFO("window " << window << ": noise " << ratio << " x rectangular, ENBW " <<
			FFTFilter::GetWindowENBW(window));
		REQUIRE(fabs(ratio / FFTFilter::GetWindowENBW(window) - 1) < 0.05);
	}

	filter->GetParameter("FFT Length").SetIntVal(0);
	g_scope->GetOscilloscopeChannel(0)->Detach(0);
}
