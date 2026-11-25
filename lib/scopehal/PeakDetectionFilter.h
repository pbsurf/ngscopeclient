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
	@author Andrew D. Zonenberg
	@brief Declaration of PeakDetectionFilter
 */
#ifndef PeakDetectionFilter_h
#define PeakDetectionFilter_h

class Peak
{
public:
	Peak(int64_t x, float y, float fwhm)
		: m_x(x)
		, m_y(y)
		, m_fwhm(fwhm)
	{}

	bool operator<(const Peak& rhs) const
	{ return (m_y < rhs.m_y); }

	//X coordinate is in base units, not scaled by timebase, since we do sub-sample interpolation
	int64_t m_x;

	float m_y;
	float m_fwhm;
};

class PeakDetector
{
public:
	PeakDetector();
	virtual ~PeakDetector();

	const std::vector<Peak>& GetPeaks()
	{ return m_peaks; }

	template<class T>
	ATTR_NOINLINE
	void FindPeaks(
		T* cap,
		int64_t max_peaks,
		float search_hz,
		bool yUnitIsDB,
		[[maybe_unused]] vk::raii::CommandBuffer& cmdBuf,
		[[maybe_unused]] std::shared_ptr<QueueHandle> queue)
	{
		//input must be analog
		AssertTypeIsAnalogWaveform(cap);

		//double start = GetTime();

		size_t nouts = cap->size();
		if( (max_peaks == 0) || (nouts < 2) )
			m_peaks.clear();
		else
		{
			//TODO: use the GPU
			cap->PrepareForCpuAccess();

			//Get peak search width in bins
			//(assume bins are equal size, this should get us close)
			int64_t binsize = GetOffsetScaled(cap, 1) - GetOffsetScaled(cap, 0);
			int64_t search_bins = ceil(search_hz / binsize);
			int64_t search_rad = search_bins/2;
			search_rad = std::max(search_rad, (int64_t)1);

			//Find peaks: bins higher than everything else within search_rad bins on either side
			struct Candidate
			{
				ssize_t index;
				int64_t x;
				float y;
			};
			std::vector<Candidate> candidates;
			const float* s = cap->m_samples.GetCpuPointer();
			ssize_t nend = nouts-1;
			ssize_t minpeak = 10;		//Skip this many bins at left to avoid false positives on the DC peak
										//(TODO: this only makes sense for FFT)

			//Adds a peak at bin i to the candidates
			auto addCandidate = [&](ssize_t i)
			{
				//Quadratic interpolate peak position
				//https://ccrma.stanford.edu/~jos/sasp/Quadratic_Interpolation_Spectral_Peaks.html
				float alpha = s[i-1];
				float beta = s[i];
				float gamma = s[i+1];
				float p = 0.5 * (alpha - gamma) / (alpha - 2*beta + gamma);
				int64_t peak_location =
					(i * cap->m_timescale) +
					static_cast<int64_t>(round(p * cap->m_timescale)) +
					cap->m_triggerPhase;

				//Interpolate peak magnitude
				float lerpMag = beta - 0.25 * (alpha - gamma) * p;

				candidates.push_back({i, peak_location, lerpMag});
			};

			//Peaks can be in bins minpeak to nend-1, and are compared against bins minpeak to nend.
			//Both searches below find the same peaks; the second is faster unless the window is only a few bins wide
			//(then most bins are peaks)
			ssize_t rad = search_rad;
			if(rad < 16)
			{
				for(ssize_t i=minpeak; i<nend; i++)
				{
					ssize_t left = std::max(minpeak, i - rad);
					ssize_t right = std::min(i + rad, nend);

					float target = s[i];
					bool is_peak = true;
					for(ssize_t j=left; j<=right; j++)
					{
						if(i == j)
							continue;
						if(s[j] >= target)
						{
							//Something higher is to our right.
							//It's higher than anything from left to j. This makes it a candidate peak.
							//Restart our search from there.
							if(j > i)
								i = j-1;

							is_peak = false;
							break;
						}
					}
					if(!is_peak)
						continue;

					addCandidate(i);

					//We know we're the highest point until at least i+search_rad.
					//Don't bother searching those points.
					i += (rad-1);
				}
			}
			else
			{
				//Split the bins peaks can be in into blocks of search_rad bins. Any two bins in a block are within
				//search_rad of each other, so only the highest bin in a block can be a peak. Find those first:
				//this loop's branch is rarely taken, unlike a test of each bin against its neighbors on a noisy
				//spectrum (which mispredicts often)
				size_t nblocks = (nend > minpeak) ? (nend - minpeak + rad - 1) / rad : 0;
				std::vector<ssize_t> blockMaxIndex(nblocks);
				std::vector<float> blockMax(nblocks);
				for(size_t b=0; b<nblocks; b++)
				{
					ssize_t start = minpeak + b*rad;
					ssize_t end = std::min(start + rad, nend);
					ssize_t imax = start;
					for(ssize_t j=start+1; j<end; j++)
					{
						if(s[j] > s[imax])
							imax = j;
					}
					blockMaxIndex[b] = imax;
					blockMax[b] = s[imax];
				}

				//A block's highest bin (the first one, if there's a tie) is a peak if nothing else within search_rad
				//is as high. That covers the rest of its own block and parts of the blocks on either side, which
				//only need searching if their highest bin is as high
				for(size_t b=0; b<nblocks; b++)
				{
					ssize_t i = blockMaxIndex[b];
					float target = s[i];
					ssize_t start = minpeak + b*rad;
					ssize_t end = std::min(start + rad, nend);

					bool higher = false;
					for(ssize_t j=i+1; j<end; j++)
						higher |= (s[j] >= target);
					if( (b > 0) && (blockMax[b-1] >= target) )
					{
						for(ssize_t j=std::max(minpeak, i - rad); j<start; j++)
							higher |= (s[j] >= target);
					}
					if( (b+1 < nblocks) && (blockMax[b+1] >= target) )
					{
						ssize_t right = std::min(i + rad, nend);
						for(ssize_t j=end; j<=right; j++)
							higher |= (s[j] >= target);
					}

					//Bin nend isn't in any block
					if( (i + rad >= nend) && (s[nend] >= target) )
						higher = true;

					if(!higher)
						addCandidate(i);
				}
			}

			//Keep the requested count, tallest first
			size_t nkeep = std::min((size_t)max_peaks, candidates.size());
			std::partial_sort(candidates.begin(), candidates.begin() + nkeep, candidates.end(),
				[](const Candidate& a, const Candidate& b) { return a.y > b.y; });

			//Only a linear Y axis needs the baseline (a full pass over the waveform)
			float baseline = 0;
			if(!yUnitIsDB)
				baseline = Filter::GetMinVoltage(cap);

			//Find the FWHM of the peaks we keep
			m_peaks.clear();
			for(size_t k=0; k<nkeep; k++)
			{
				auto& c = candidates[k];
				ssize_t i = c.index;

				//Move left and right from the peak until we get half magnitude
				//If Y axis is dB, we want to be half *magnitude* not half dB
				float hmtarget;
				if(yUnitIsDB)
					hmtarget = c.y - 3;
				else
					hmtarget = (c.y - baseline)/2 + baseline;
				ssize_t hmleft = i;
				ssize_t hmright = i;
				for(ssize_t j=i; j >= 0; j--)
				{
					//TODO: interpolate
					if(s[j] <= hmtarget)
					{
						 hmleft = j;
						 break;
					}
				}
				for(ssize_t j=i; j < (ssize_t)nouts; j++)
				{
					//TODO: interpolate
					if(s[j] <= hmtarget)
					{
						 hmright = j;
						 break;
					}
				}
				float fwhm = GetOffsetScaled(cap, hmright) - GetOffsetScaled(cap, hmleft);

				m_peaks.push_back(Peak(c.x, c.y, fwhm));
			}
		}

		//double dt = GetTime() - start;
		//LogDebug("delta = %.3f ms\n", dt * 1000);
	}

protected:
	std::vector<Peak> m_peaks;

	AcceleratorBuffer<float> m_filteredInput;
	AcceleratorBuffer<float> m_peakCoefficients;

	ComputePipeline m_peakFirComputePipeline;
};

/**
	@brief A filter that does peak detection
 */
class PeakDetectionFilter
	: public Filter
	, public PeakDetector
{
public:
	PeakDetectionFilter(const std::string& color, Category cat);
	virtual ~PeakDetectionFilter();

	///@brief Number of peaks to label. GetPeaks() holds more: candidates for the labels, and maybe more for downstream filters
	size_t GetDisplayedPeakCount()
	{ return std::min(static_cast<size_t>(std::max(m_numpeaks.GetIntVal(), (int64_t)0)), m_peaks.size()); }

	///@brief Maximum number of peaks kept when a downstream filter consumes the peak list
	static constexpr int64_t MAX_STORED_PEAKS = 1000;

	/**
		@brief Number of peaks kept beyond Number of Peaks, so the display can tell when a labeled peak drops out of the
		top few only briefly (as peaks do on a noisy spectrum), and keep its label
	 */
	static constexpr int64_t EXTRA_LABEL_CANDIDATES = 16;

protected:

	bool HasPeakConsumer();

	/**
		@brief Checks if the peak search should run: peaks are displayed, or a downstream filter reads them

		Derived classes that skip FindPeaks() (and the GPU sync it needs) when it isn't wanted must test this,
		not just the Number of Peaks parameter.
	 */
	bool IsPeakSearchNeeded()
	{ return (m_numpeaks.GetIntVal() > 0) || HasPeakConsumer(); }

	template<class T>
	void FindPeaks(T* cap, vk::raii::CommandBuffer& cmdBuf, std::shared_ptr<QueueHandle> queue)
	{
		//Only search if we display peaks or a downstream filter reads them (the search is slow).
		//When displaying peaks, keep a few more as candidates for the labels (the search finds them all anyway).
		//If a downstream filter reads them, keep every peak found (up to a cap) so it isn't limited to the displayed ones.
		int64_t maxPeaks = std::max(m_numpeaks.GetIntVal(), (int64_t)0);
		if(maxPeaks > 0)
			maxPeaks += EXTRA_LABEL_CANDIDATES;
		if(HasPeakConsumer())
			maxPeaks = std::max(maxPeaks, MAX_STORED_PEAKS);

		PeakDetector::FindPeaks(
			cap,
			maxPeaks,
			GetPeakWindow(),
			GetYAxisUnits(0).IsLogarithmic(),
			cmdBuf,
			queue);
	}

	///@brief Gets the Peak Window parameter (in Hz) in X axis units
	float GetPeakWindow()
	{
		float window = m_peakwindow.GetFloatVal();
		if(m_xAxisUnit.GetType() == Unit::UNIT_MICROHZ)
			window *= 1e6;
		return window;
	}

	FilterParameter& m_numpeaks;
	FilterParameter& m_peakwindow;
};

#endif

