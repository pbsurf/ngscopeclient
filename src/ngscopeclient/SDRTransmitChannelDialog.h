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
	@brief Declaration of SDRTransmitChannelDialog
 */
#ifndef SDRTransmitChannelDialog_h
#define SDRTransmitChannelDialog_h

#include "EmbeddableDialog.h"
#include "../scopehal/SDRTransmitChannel.h"

class MainWindow;
class FileBrowser;
class SCPISDR;

/**
	@brief Properties of a transmit path of an SDR: display settings, and the transmit calibration

	The settings that change often (gain, tones) are in the stream browser, like the receive gain and frequency.
 */
class SDRTransmitChannelDialog : public EmbeddableDialog
{
public:
	SDRTransmitChannelDialog(SDRTransmitChannel* chan, MainWindow* parent, bool graphEditorMode = false);
	virtual ~SDRTransmitChannelDialog();

	virtual bool Render() override;
	virtual bool DoRender() override;

	void RunFileDialog();

	SDRTransmitChannel* GetChannel()
	{ return m_channel; }

protected:
	SCPISDR* GetSDR();
	void RenderCalibration(SCPISDR* sdr, float width);

	SDRTransmitChannel* m_channel;

	std::string m_displayName;
	std::string m_committedDisplayName;

	float m_color[3];

	///@brief Gain after the output, as edited and as last applied
	std::string m_externalGain;
	float m_committedExternalGain;

	///@brief Calibration file or JSON, as edited and as last applied
	std::string m_calFile;
	std::string m_committedCalFile;

	///@brief Browser for the calibration file, if it's open
	std::shared_ptr<FileBrowser> m_fileDialog;
};

#endif
