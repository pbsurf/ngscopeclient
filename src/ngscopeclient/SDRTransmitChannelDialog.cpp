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
	@brief Implementation of SDRTransmitChannelDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "SDRTransmitChannelDialog.h"
#include "FileBrowser.h"
#include "../scopehal/SCPISDR.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SDRTransmitChannelDialog::SDRTransmitChannelDialog(
	SDRTransmitChannel* chan,
	MainWindow* parent,
	bool graphEditorMode)
	: EmbeddableDialog(
		chan->GetHwname(),
		string("Channel properties: ") + chan->GetHwname(),
		ImVec2(300, 400),
		parent,
		graphEditorMode)
	, m_channel(chan)
	, m_committedExternalGain(0)
{
	m_session = &parent->GetSession();
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Color
	auto color = ColorFromString(m_channel->m_displaycolor);
	m_color[0] = ((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
	m_color[1] = ((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
	m_color[2] = ((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;

	auto sdr = GetSDR();
	if(sdr)
	{
		size_t tx = m_channel->GetTxIndex();
		m_committedExternalGain = sdr->GetTxExternalGain(tx);
		m_externalGain = Unit(Unit::UNIT_DB).PrettyPrint(m_committedExternalGain);
		m_committedCalFile = sdr->GetTxCalibrationFile(tx);
		m_calFile = m_committedCalFile;
	}
}

SDRTransmitChannelDialog::~SDRTransmitChannelDialog()
{
}

///@brief Gets the radio this transmit path is part of
SCPISDR* SDRTransmitChannelDialog::GetSDR()
{
	return dynamic_cast<SCPISDR*>(m_channel->GetInstrument());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool SDRTransmitChannelDialog::Render()
{
	RunFileDialog();
	return Dialog::Render();
}

/**
	@brief Runs the calibration file browser, if it's open
 */
void SDRTransmitChannelDialog::RunFileDialog()
{
	if(!m_fileDialog)
		return;

	m_fileDialog->Render();

	if(m_fileDialog->IsClosedOK())
	{
		m_committedCalFile = m_fileDialog->GetFileName();
		m_calFile = m_committedCalFile;

		auto sdr = GetSDR();
		if(sdr)
			sdr->SetTxCalibrationFile(m_channel->GetTxIndex(), m_committedCalFile);
	}

	if(m_fileDialog->IsClosed())
		m_fileDialog = nullptr;
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool SDRTransmitChannelDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	float width = 10 * ImGui::GetFontSize();

	auto inst = m_channel->GetInstrument();
	if(!inst)
		return true;

	if(ImGui::CollapsingHeader("Info"))
	{
		auto nickname = inst->m_nickname;
		auto hwname = m_channel->GetHwname();
		auto index = to_string(m_channel->GetTxIndex() + 1);	//use one based index for display

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Instrument", &nickname);
		ImGui::EndDisabled();
		HelpMarker("The radio this transmit path is part of");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Transmit path", &index);
		ImGui::EndDisabled();
		HelpMarker("Transmit path number (starting from 1) on the radio");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Hardware Name", &hwname);
		ImGui::EndDisabled();
		HelpMarker("Hardware name for the channel (as used in the instrument API)");
	}

	if(ImGui::CollapsingHeader("Display", defaultOpenFlags))
	{
		ImGui::SetNextItemWidth(width);
		if(TextInputWithImplicitApply("Nickname", m_displayName, m_committedDisplayName))
			m_channel->SetDisplayName(m_committedDisplayName);

		HelpMarker("Display name for the channel");

		if(ImGui::ColorEdit3(
			"Color",
			m_color,
			ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8))
		{
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "#%02x%02x%02x",
				static_cast<int>(round(m_color[0] * 255)),
				static_cast<int>(round(m_color[1] * 255)),
				static_cast<int>(round(m_color[2] * 255)));
			m_channel->m_displaycolor = tmp;
		}
	}

	auto sdr = GetSDR();
	if(sdr && ImGui::CollapsingHeader("Calibration", defaultOpenFlags))
		RenderCalibration(sdr, width);

	return true;
}

/**
	@brief Renders the external gain and calibration file of the transmit path, and the full scale level of each tone
 */
void SDRTransmitChannelDialog::RenderCalibration(SCPISDR* sdr, float width)
{
	Unit db(Unit::UNIT_DB);
	size_t tx = m_channel->GetTxIndex();

	ImGui::SetNextItemWidth(width);
	if(UnitInputWithImplicitApply("External gain", m_externalGain, m_committedExternalGain, db))
		sdr->SetTxExternalGain(tx, m_committedExternalGain);
	HelpMarker(
		"Gain of whatever is after the output, such as an amplifier or a cable. Use a negative value for loss, "
		"for example -20 dB for a 20 dB attenuator.\n\n"
		"Tone levels are shown as they are at the output of that, rather than at the output of the radio.");

	ImGui::SetNextItemWidth(width);
	if(TextInputWithImplicitApply("###calfile", m_calFile, m_committedCalFile))
		sdr->SetTxCalibrationFile(tx, m_committedCalFile);
	ImGui::SameLine();
	if(ImGui::Button("...###calbrowser"))
	{
		if(!m_fileDialog)
		{
			vector<FileBrowserFilter> filters =
			{
				{ "Calibration files (*.csv, *.txt, *.s2p, *.json)", "*.csv;*.txt;*.s2p;*.json" },
				{ "All files", "*" }
			};

			//Don't start the browser in pasted JSON
			string start = SCPISDR::IsCalibrationJson(m_committedCalFile) ? "" : m_committedCalFile;
			m_fileDialog = MakeFileBrowser(m_parent, start, "Select Transmit Calibration File", filters, false);
		}
		else
			LogTrace("file dialog is already open, ignoring additional button click\n");
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("Calibration file");
	HelpMarker(
		"Output level of the radio vs frequency, so tone levels can be shown and set in dBm. Leave empty for no "
		"calibration.\n\n"
		"Each point is the level in dBm of a single tone at 100% amplitude and 0 dB gain, at an RF frequency (TX LO "
		"plus tone frequency). It's interpolated linearly between points. To measure a point, turn on one tone at a "
		"moderate setting (for example -10 dB gain and 50%), measure its level, and add back the attenuation and "
		"amplitude (here +10 dB and +6.02 dB).\n\n"
		"Either a text file with a frequency and a level on each line, for example\n"
		"    # freq, level\n"
		"    100 MHz, 6.5 dBm\n"
		"    2.4e9, 4.2\n"
		"(frequency in Hz, or with a unit), a Touchstone file with S21 as the level, or a JSON file.\n\n"
		"JSON can also be pasted straight into the box instead of a path. It's an array of points, each either "
		"[frequency, level] or {\"freq\": frequency, \"gain\": level}, or an object with that array under "
		"\"points\", for example\n"
		"    [[100e6, 6.5], [\"2.4 GHz\", 4.2]]");

	auto err = sdr->GetTxCalibrationError(tx);
	if(!err.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4, 0.4, 1));
		ImGui::TextWrapped("%s", err.c_str());
		ImGui::PopStyleColor();
	}

	//Show what that adds up to for each tone
	if(!sdr->HasTxCalibration(tx))
		return;
	Unit hz(Unit::UNIT_HZ);
	Unit dbm(Unit::UNIT_DBM);
	int64_t lo = sdr->GetTxLOFrequency();
	for(size_t j=0; j<sdr->GetTxToneCount(tx); j++)
	{
		ImGui::TextDisabled("Tone %zu full scale at %s: %s",
			j+1,
			hz.PrettyPrint(static_cast<double>(lo) + sdr->GetTxToneFrequency(tx, j)).c_str(),
			dbm.PrettyPrint(sdr->GetTxToneFullScaleLevel(tx, j)).c_str());
	}
	HelpMarker(
		"Level each tone would have at 100% amplitude and 0 dB gain, at its RF frequency: the calibration plus the "
		"external gain.");
}
