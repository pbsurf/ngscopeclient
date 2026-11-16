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
	@brief Implementation of PeakTableDialog
 */

#include "ngscopeclient.h"
#include "PeakTableDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PeakTableDialog::PeakTableDialog(PeakDetectionFilter* filter, Session* session, MainWindow* parent)
	: Dialog(
		string("Peaks: ") + filter->GetDisplayName(),
		string("Peaks: ") + filter->GetHwname(),
		ImVec2(450, 300),
		session,
		parent)
{
	CreateInput("filter");
	SetInput(0, StreamDescriptor(filter, 0));
}

PeakTableDialog::~PeakTableDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed (or the filter was deleted)
 */
bool PeakTableDialog::DoRender()
{
	auto f = GetFilter();
	if(!f)
		return false;

	//Keep title in sync
	m_title = string("Peaks: ") + f->GetDisplayName();

	//How many are labeled. It's the filter's parameter, so it applies to every plot of the filter.
	auto& param = f->GetParameter("Number of Peaks");
	int n = static_cast<int>(max(param.GetIntVal(), (int64_t)0));
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
	if(ImGui::InputInt("Labeled peaks", &n))
	{
		n = max(n, 0);
		param.SetIntVal(n);

		//Re-run so the candidates for the new count are there even if acquisition is stopped
		m_session->RefreshAllFiltersNonblocking();
	}
	HelpMarker(
		"How many peaks are labeled on the plots of this filter (its Number of Peaks). The table also shows the "
		"next few peaks, dimmed.");

	//Copy the peaks, under the same lock the plots read them with
	vector<Row> rows;
	Unit xunit(Unit::UNIT_HZ);
	Unit yunit(Unit::UNIT_DB);
	int64_t binsize = 0;
	{
		shared_lock<shared_mutex> lock(m_session->GetWaveformDataMutex());
		auto stream = StreamDescriptor(f, 0);
		xunit = stream.GetXAxisUnits();
		yunit = stream.GetYAxisUnits();
		auto uwfm = dynamic_cast<UniformWaveformBase*>(stream.GetData());
		binsize = uwfm ? uwfm->m_timescale : 0;

		auto& peaks = f->GetPeaks();
		size_t nrows = min(peaks.size(), static_cast<size_t>(n) + PeakDetectionFilter::EXTRA_LABEL_CANDIDATES);
		for(size_t i=0; i<nrows; i++)
			rows.push_back({i+1, peaks[i].m_x, peaks[i].m_y, peaks[i].m_fwhm, false});
	}

	//Which ones the plots label. The labels have hysteresis, so this isn't always the first n by rank.
	auto labeled = m_parent->GetPeakLabelPositions(f);
	for(auto& r : rows)
		r.m_labeled = (labeled.find(r.m_x) != labeled.end());

	ImGui::SameLine();
	if(ImGui::Button("Copy"))
		CopyAsCSV(rows, xunit, yunit);
	Tooltip("Copy the table to the clipboard as CSV");

	if(rows.empty())
	{
		ImGui::TextDisabled("No peaks");
		return true;
	}

	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_SizingFixedFit;

	enum Columns
	{
		COL_RANK,
		COL_X,
		COL_Y,
		COL_DELTA,
		COL_FWHM
	};

	float width = ImGui::GetFontSize();
	if(!ImGui::BeginTable("peaks", 5, flags))
		return true;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 3*width, COL_RANK);
	ImGui::TableSetupColumn(xunit.GetType() == Unit::UNIT_HZ ? "Frequency" : "X",
		ImGuiTableColumnFlags_WidthFixed, 8*width, COL_X);
	ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 6*width, COL_Y);
	ImGui::TableSetupColumn("Δ highest", ImGuiTableColumnFlags_WidthFixed, 6*width, COL_DELTA);
	ImGui::TableSetupColumn("FWHM", ImGuiTableColumnFlags_WidthFixed, 6*width, COL_FWHM);
	ImGui::TableHeadersRow();

	//Deltas are to the highest peak, which is first by rank
	float highest = rows[0].m_y;
	bool logY = yunit.IsLogarithmic();
	Unit deltaUnit = logY ? Unit(Unit::UNIT_DB) : yunit;

	//Sort as asked (rank 1 is the highest peak)
	auto specs = ImGui::TableGetSortSpecs();
	if(specs && (specs->SpecsCount > 0))
	{
		auto& spec = specs->Specs[0];
		bool asc = (spec.SortDirection == ImGuiSortDirection_Ascending);
		auto col = spec.ColumnUserID;
		auto key = [&](const Row& r) -> double
		{
			switch(col)
			{
				case COL_X:
					return static_cast<double>(r.m_x);
				case COL_FWHM:
					return r.m_fwhm;

				//Level and delta sort by level
				case COL_Y:
				case COL_DELTA:
					return r.m_y;

				default:
					return static_cast<double>(r.m_rank);
			}
		};
		stable_sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b)
			{ return asc ? (key(a) < key(b)) : (key(a) > key(b)); });
	}

	for(auto& r : rows)
	{
		ImGui::PushID(static_cast<int>(r.m_rank));
		ImGui::TableNextRow(ImGuiTableRowFlags_None);

		if(!r.m_labeled)
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

		ImGui::TableSetColumnIndex(COL_RANK);
		string rank = to_string(r.m_rank) + (r.m_labeled ? " *" : "");
		if(ImGui::Selectable(rank.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			m_parent->CenterOnPeak(f, r.m_x);
		if(ImGui::IsItemHovered())
		{
			m_parent->SetHighlightedPeak(f, r.m_x, r.m_y);
			m_parent->AddStatusHelp("mouse_lmb", "Center the plot on this peak");
		}

		ImGui::TableSetColumnIndex(COL_X);
		ImGui::TextUnformatted(xunit.PrettyPrintInt64WithResolution(r.m_x, binsize / 10.0).c_str());

		ImGui::TableSetColumnIndex(COL_Y);
		if(logY)
			ImGui::TextUnformatted(yunit.PrettyPrintTabular(r.m_y, 0, 2).c_str());
		else
			ImGui::TextUnformatted(yunit.PrettyPrint(r.m_y, 4).c_str());

		ImGui::TableSetColumnIndex(COL_DELTA);
		float delta = r.m_y - highest;
		if(logY)
			ImGui::TextUnformatted(deltaUnit.PrettyPrintTabular(delta, 0, 2).c_str());
		else
			ImGui::TextUnformatted(deltaUnit.PrettyPrint(delta, 4).c_str());

		//Widths are measured to the first bin at or below half maximum on each side (see the plot labels)
		ImGui::TableSetColumnIndex(COL_FWHM);
		string fwhm;
		if(binsize > 0)
		{
			fwhm = (r.m_fwhm <= 2*binsize) ? "< " : "";
			fwhm += xunit.PrettyPrintInt64WithResolution(llround(r.m_fwhm), binsize);
		}
		else
			fwhm = xunit.PrettyPrint(r.m_fwhm);
		ImGui::TextUnformatted(fwhm.c_str());

		if(!r.m_labeled)
			ImGui::PopStyleColor();

		ImGui::PopID();
	}

	ImGui::EndTable();

	return true;
}

/**
	@brief Copies the rows (in the order shown) to the clipboard as CSV, with plain numbers in the units of the axes
 */
void PeakTableDialog::CopyAsCSV(const vector<Row>& rows, Unit xunit, Unit yunit)
{
	string xname = xunit.ToString();
	string yname = yunit.ToString();
	string csv = "rank,x (" + xname + "),level (" + yname + "),fwhm (" + xname + "),labeled\n";
	char line[256];
	for(auto& r : rows)
	{
		snprintf(line, sizeof(line), "%zu,%" PRId64 ",%.3f,%.0f,%d\n",
			r.m_rank, r.m_x, r.m_y, r.m_fwhm, r.m_labeled ? 1 : 0);
		csv += line;
	}
	ImGui::SetClipboardText(csv.c_str());
}
