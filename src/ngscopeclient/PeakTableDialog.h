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
	@author ngscopeclient contributors
	@brief Declaration of PeakTableDialog
 */
#ifndef PeakTableDialog_h
#define PeakTableDialog_h

#include "Dialog.h"
#include "../scopehal/SinkNode.h"
#include "../scopehal/PeakDetectionFilter.h"

class MainWindow;

/**
	@brief Table of the peaks found by one peak detecting filter (FFT, Spectrum Stitch, Peaks, etc)

	Shows the peaks the plots label, and the next few candidates, and sets how many are labeled (the filter's Number
	of Peaks). Hovering a row highlights the peak on the plots, clicking it centers them on it.

	The dialog is a sink of the filter, so it goes away when the filter is deleted, and the filter keeps searching for
	peaks while it's open even if none are labeled.
 */
class PeakTableDialog
	: public Dialog
	, public SinkNode
{
public:
	PeakTableDialog(PeakDetectionFilter* filter, Session* session, MainWindow* parent);
	virtual ~PeakTableDialog();

	virtual bool DoRender() override;

	virtual bool ConsumesUpstreamPeaks() override
	{ return true; }

	PeakDetectionFilter* GetFilter()
	{ return dynamic_cast<PeakDetectionFilter*>(GetInput(0).m_channel); }

protected:
	///@brief A row of the table
	struct Row
	{
		size_t m_rank;
		int64_t m_x;
		float m_y;
		float m_fwhm;
		bool m_labeled;
	};

	void CopyAsCSV(const std::vector<Row>& rows, Unit xunit, Unit yunit);
};

#endif
