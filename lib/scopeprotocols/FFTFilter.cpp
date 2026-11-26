/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
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

#include "../scopehal/scopehal.h"
#include "../scopehal/AlignedAllocator.h"
#include "FFTFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FFTFilter::FFTFilter(const string& color)
	: PeakDetectionFilter(color, CAT_RF)
	, m_windowName("Window")
	, m_detectorName("Detector")
	, m_fftLength(m_parameters["FFT Length"])
	, m_blackmanHarrisComputePipeline("shaders/BlackmanHarrisWindow.spv", 2, sizeof(WindowFunctionArgs))
	, m_rectangularComputePipeline("shaders/RectangularWindow.spv", 2, sizeof(WindowFunctionArgs))
	, m_cosineSumComputePipeline("shaders/CosineSumWindow.spv", 2, sizeof(WindowFunctionArgs))
	, m_complexToMagnitudeComputePipeline("shaders/ComplexToMagnitude.spv", 2, sizeof(ComplexToMagnitudeArgs))
	, m_complexToLogMagnitudeComputePipeline("shaders/ComplexToLogMagnitude.spv", 2, sizeof(ComplexToMagnitudeArgs))
	, m_averageLogMagnitudeComputePipeline(
		"shaders/ComplexToLogMagnitudeAverage.spv", 2, sizeof(AverageLogMagnitudeArgs))
{
	m_xAxisUnit = Unit(Unit::UNIT_MICROHZ);
	AddStream(Unit(Unit::UNIT_DBM), "data", Stream::STREAM_TYPE_ANALOG);

	//Set up channels
	CreateInput<InputConstraintStreamType>("din", Stream::STREAM_TYPE_ANALOG);

	m_cachedNumPoints = 0;
	m_cachedNumPointsFFT = 0;
	m_cachedNumOuts = 0;
	m_cachedNumSegments = 0;

	//Default config
	m_range = 70;
	m_offset = 35;

	m_parameters[m_windowName] = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_windowName].AddEnumValue("Blackman-Harris", WINDOW_BLACKMAN_HARRIS);
	m_parameters[m_windowName].AddEnumValue("Hamming", WINDOW_HAMMING);
	m_parameters[m_windowName].AddEnumValue("Hann", WINDOW_HANN);
	m_parameters[m_windowName].AddEnumValue("Rectangular", WINDOW_RECTANGULAR);
	m_parameters[m_windowName].SetIntVal(WINDOW_HAMMING);

	m_parameters[m_detectorName] = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_detectorName].AddEnumValue("Normal", DETECTOR_NORMAL);
	m_parameters[m_detectorName].AddEnumValue("Peak", DETECTOR_PEAK);
	m_parameters[m_detectorName].AddEnumValue("Average", DETECTOR_AVERAGE);
	m_parameters[m_detectorName].SetIntVal(DETECTOR_NORMAL);

	//FFT length. Shorter than the input, the input is split into segments overlapping by half, and the power in
	//each bin is averaged over the segments (Welch's method). Bins are wider, so the noise in each is higher, but a
	//tone has the same level.
	m_fftLength = FilterParameter(FilterParameter::TYPE_ENUM, Unit(Unit::UNIT_COUNTS));
	m_fftLength.AddEnumValue("Whole input", 0);
	for(int64_t len = 1024; len <= 1048576; len *= 2)
		m_fftLength.AddEnumValue(to_string(len), len);
	m_fftLength.SetIntVal(0);
}

FFTFilter::~FFTFilter()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

float FFTFilter::GetOffset(size_t /*stream*/)
{
	return m_offset;
}

float FFTFilter::GetVoltageRange(size_t /*stream*/)
{
	return m_range;
}

void FFTFilter::SetVoltageRange(float range, size_t /*stream*/)
{
	m_range = range;
}

void FFTFilter::SetOffset(float offset, size_t /*stream*/)
{
	m_offset = offset;
}

string FFTFilter::GetProtocolName()
{
	return "FFT";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

uint32_t FFTFilter::GetExecutionCapabilitiesMask()
{
	if(IsPeakSearchNeeded())
	{
		return
			(uint32_t)ExecutionCapabilities::CommandBufferAppend |
			(uint32_t)ExecutionCapabilities::VulkanOnly;
	}
	else
	{
		return
			(uint32_t)ExecutionCapabilities::CommandBufferAppend |
			(uint32_t)ExecutionCapabilities::CommandBufferTailCall |
			(uint32_t)ExecutionCapabilities::VulkanOnly;
	}
}

/**
	@brief Reallocates buffers and makes a new FFT plan

	Only called when the input size, FFT size or segment count changes.

	@param npoints_raw	Number of input samples
	@param npoints		Number of points in each FFT
	@param nouts		Number of output bins of each FFT
	@param nsegments	Number of FFTs (batched in one plan)
 */
void FFTFilter::ReallocateBuffers(size_t npoints_raw, size_t npoints, size_t nouts, size_t nsegments)
{
	m_cachedNumPoints = npoints_raw;
	m_cachedNumPointsFFT = npoints;
	m_cachedNumSegments = nsegments;

	m_rdinbuf.SetCpuAccessHint(AcceleratorBuffer<float>::HINT_NEVER);
	m_rdinbuf.SetGpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);
	m_rdoutbuf.SetCpuAccessHint(AcceleratorBuffer<float>::HINT_NEVER);
	m_rdoutbuf.SetGpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);

	m_vkPlan = nullptr;
	m_vkPlan = make_unique<VulkanFFTPlan>(npoints, nouts, VulkanFFTPlan::DIRECTION_FORWARD, nsegments);

	//Each segment is npoints real values in, nouts complex values out
	m_rdinbuf.resize(npoints * nsegments);
	m_rdoutbuf.resize(2 * nouts * nsegments);
}

/**
	@brief Works out the FFT size and segments for an input of ninput samples, from the FFT Length setting

	The whole input is one FFT. A shorter FFT length splits the input into segments of that length overlapping by
	half; the samples after the last whole segment are not used.
 */
void FFTFilter::GetSegments(size_t ninput, size_t& npoints, size_t& nsegments, size_t& hop)
{
	npoints = ninput;
	nsegments = 1;
	hop = 0;
	auto fftlen = static_cast<size_t>(m_fftLength.GetIntVal());
	if( (fftlen > 0) && (fftlen < ninput) )
	{
		npoints = fftlen;
		hop = fftlen / 2;
		nsegments = (ninput - fftlen) / hop + 1;
	}
}

void FFTFilter::Refresh(vk::raii::CommandBuffer& cmdBuf, shared_ptr<QueueHandle> queue)
{
	#ifdef HAVE_NVTX
		nvtx3::scoped_range nrange("FFTFilter::Refresh");
	#endif

	//Make sure we've got valid inputs
	ClearMessages();
	auto din = dynamic_cast<UniformAnalogWaveform*>(GetInputWaveform(0));
	if(!din)
	{
		if(!GetInput(0))
			AddErrorMessage("Missing inputs", "No signal input connected");
		else if(!GetInputWaveform(0))
			AddErrorMessage("Missing inputs", "No waveform available at input");
		else
			AddErrorMessage("Invalid inputs", "Expect a uniform analog input");

		SetData(nullptr, 0);
		return;
	}

	//Force unit to uHz at this point, because legacy scopesessions with unit of Hz need to be converted
	m_xAxisUnit = Unit(Unit::UNIT_MICROHZ);

	//An empty input (e.g. a session loaded offline with no waveform data) has nothing to transform.
	//Bail out now, since we'd never create an FFT plan for it (the size matches our initial cached value of zero)
	const size_t ninput = din->size();
	if(ninput == 0)
	{
		AddErrorMessage("Missing inputs", "Input waveform is empty");
		SetData(nullptr, 0);
		return;
	}
	LogTrace("FFTFilter: processing %zu input samples\n", ninput);

	//FFT the whole input, or average segments of the FFT length overlapping by half
	size_t npoints;
	size_t nsegments;
	size_t hop;
	GetSegments(ninput, npoints, nsegments, hop);

	//Reallocate buffers if size has changed
	const size_t nouts = npoints/2 + 1;
	m_cachedNumOuts = nouts;
	if( (m_cachedNumPoints != ninput) || (m_cachedNumPointsFFT != npoints) || (m_cachedNumSegments != nsegments) ||
		!m_vkPlan)
	{
		ReallocateBuffers(ninput, npoints, nouts, nsegments);
	}
	LogTrace("Output: %zu\n", nouts);

	DoRefresh(din, din->m_samples, din->m_timescale, npoints, nouts, true, cmdBuf, queue, nsegments, hop);
}

void FFTFilter::DoRefresh(
	WaveformBase* din,
	AcceleratorBuffer<float>& data,
	double fs_per_sample,
	size_t npoints,
	size_t nouts,
	bool log_output,
	vk::raii::CommandBuffer& cmdBuf,
	shared_ptr<QueueHandle> queue,
	size_t nsegments,
	size_t hop)
{
	//Look up some parameters.
	//The bins are the sample rate / npoints apart (nouts is npoints/2 + 1, so it can't be used for this)
	double sample_ghz = 1e6 / fs_per_sample;
	double bin_uhz_raw = sample_ghz * 1e15 / npoints;
	LogTrace("sample_ghz = %f, bin_uhz_raw = %f\n", sample_ghz, bin_uhz_raw);
	int64_t bin_uhz = round(bin_uhz_raw);
	auto window = static_cast<WindowFunction>(m_parameters[m_windowName].GetIntVal());
	LogTrace("bin_uhz: %" PRIi64 " (%s)\n", bin_uhz, Unit(Unit::UNIT_MICROHZ).PrettyPrint(bin_uhz).c_str());

	//Set up output and copy time scales / configuration
	auto cap = SetupEmptyUniformAnalogOutputWaveform(din, 0);
	cap->m_triggerPhase = 0;	//first peak is DC
	cap->m_timescale = bin_uhz;
	cap->Resize(nouts);

	//If we have too big a FFT at low sample rate we can run below 1 Hz resolution and bork
	if(cap->m_timescale == 0)
	{
		SetData(nullptr, 0);
		return;
	}

	//Output scale is based on the number of points we FFT that contain actual sample data.
	//(If we're zero padding, the zeroes don't contribute any power)
	size_t numActualSamples = min(data.size(), npoints);
	float scale = sqrt(2.0) / numActualSamples;

	//We also need to adjust the scale by the coherent power gain of the window function
	switch(window)
	{
		case WINDOW_HAMMING:
			scale *= 1.862;
			break;

		case WINDOW_HANN:
			scale *= 2.013;
			break;

		case WINDOW_BLACKMAN_HARRIS:
			scale *= 2.805;
			break;

		//unit
		case WINDOW_RECTANGULAR:
		default:
			break;
	}

	//Configure the window
	WindowFunctionArgs args;
	args.numActualSamples = numActualSamples;
	args.npoints = npoints;
	args.scale = 2 * M_PI / numActualSamples;
	args.offsetIn = 0;
	args.offsetOut = 0;
	switch(window)
	{
		case WINDOW_HANN:
			args.alpha0 = 0.5;
			break;

		case WINDOW_HAMMING:
			args.alpha0 = 25.0f / 46;
			break;

		default:
			args.alpha0 = 0;
			break;
	}
	args.alpha1 = 1 - args.alpha0;

	{
		NamedDebugRange debugRange(cmdBuf, "FFTFilter");
		const uint32_t compute_block_count = GetComputeBlockCount(npoints, 64);

		//Apply the window function
		{
			NamedDebugRange shaderRange(cmdBuf, "Window function");

			ComputePipeline* wpipe = nullptr;
			switch(window)
			{
				case WINDOW_BLACKMAN_HARRIS:
					wpipe = &m_blackmanHarrisComputePipeline;
					break;

				case WINDOW_HANN:
				case WINDOW_HAMMING:
					wpipe = &m_cosineSumComputePipeline;
					break;

				default:
				case WINDOW_RECTANGULAR:
					wpipe = &m_rectangularComputePipeline;
					break;
			}
			wpipe->BindBufferNonblocking(0, data, cmdBuf);
			wpipe->BindBufferNonblocking(1, m_rdinbuf, cmdBuf, true);
			for(size_t i=0; i<nsegments; i++)
			{
				args.offsetIn = i * hop;
				args.offsetOut = i * npoints;
				if(i == 0)
				{
					wpipe->Dispatch(cmdBuf, args,
						min(compute_block_count, 32768u),
						compute_block_count / 32768 + 1);
				}
				else
				{
					wpipe->DispatchNoRebind(cmdBuf, args,
						min(compute_block_count, 32768u),
						compute_block_count / 32768 + 1);
				}
			}
			wpipe->AddComputeMemoryBarrier(cmdBuf);
			m_rdinbuf.MarkModifiedFromGpu();
		}

		//Do the actual FFT operation
		{
			NamedDebugRange shaderRange(cmdBuf, "FFT");
			m_vkPlan->AppendForward(m_rdinbuf, m_rdoutbuf, cmdBuf);
		}

		//Convert complex to real
		{
			NamedDebugRange shaderRange(cmdBuf, "Postprocess");

			//One thread per output bin (about half the FFT points)
			const uint32_t out_block_count = GetComputeBlockCount(nouts, 64);
			const float impedance = 50;

			//Average the power of the segments (only log output: JitterSpectrumFilter always uses one segment)
			if(nsegments > 1)
			{
				AverageLogMagnitudeArgs cargs;
				cargs.npoints = nouts;
				cargs.nsegments = nsegments;
				cargs.scale = scale * scale / impedance;

				auto& pipe = m_averageLogMagnitudeComputePipeline;
				pipe.BindBufferNonblocking(0, m_rdoutbuf, cmdBuf);
				pipe.BindBufferNonblocking(1, cap->m_samples, cmdBuf, true);
				pipe.AddComputeMemoryBarrier(cmdBuf);
				pipe.Dispatch(cmdBuf, cargs,
					min(out_block_count, 32768u),
					out_block_count / 32768 + 1);
			}
			else
			{
				ComputePipeline& pipe = log_output ?
					m_complexToLogMagnitudeComputePipeline : m_complexToMagnitudeComputePipeline;
				ComplexToMagnitudeArgs cargs;
				cargs.npoints = nouts;
				if(log_output)
					cargs.scale = scale * scale / impedance;
				else
					cargs.scale = scale;
				pipe.BindBufferNonblocking(0, m_rdoutbuf, cmdBuf);
				pipe.BindBufferNonblocking(1, cap->m_samples, cmdBuf, true);
				pipe.AddComputeMemoryBarrier(cmdBuf);
				pipe.Dispatch(cmdBuf, cargs,
					min(out_block_count, 32768u),
					out_block_count / 32768 + 1);
			}
		}
	}

	cap->MarkModifiedFromGpu();

	//If doing peak detection, block now
	if(IsPeakSearchNeeded())
	{
		cmdBuf.end();
		queue->SubmitAndBlock(cmdBuf);

		//Peak search (for now this runs on the CPU)
		FindPeaks(cap, cmdBuf, queue);
	}
}
