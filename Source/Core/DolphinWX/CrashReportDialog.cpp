// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Core/CrashDump.h"
#include "DolphinWX/CrashReportDialog.h"

namespace
{

// Returns a human-readable description of what data is contained in the dump
// as a newline separated string to show in the UI.
std::string DescribeDumpData(const CrashDump* dump)
{
	// TODO(delroth): Implement when we have more than just a stub. For now,
	// return fake data.
	return "Credit card number\nSSN\nBrowser history\nDisk dump\n";
}

}  // namespace

CrashReportDialog::CrashReportDialog(wxWindow* parent,
                                     std::unique_ptr<CrashDump> dump)
	: wxDialog(), m_dump(std::move(dump))
{
	Create(parent, -1, _("Send crash report"));


	wxBoxSizer* toplevel_sizer = new wxBoxSizer(wxVERTICAL);
	toplevel_sizer->AddSpacer(5);

	wxBoxSizer* details_sizer = new wxBoxSizer(wxHORIZONTAL);
	toplevel_sizer->Add(details_sizer, 1, wxEXPAND);
		details_sizer->AddSpacer(5);

		wxBoxSizer* dump_data_sizer = new wxBoxSizer(wxVERTICAL);
		details_sizer->Add(dump_data_sizer, 0, wxEXPAND);

			wxStaticText* dump_data_lbl = new wxStaticText(
			    this, -1, _("Automatically collected information"));
			dump_data_sizer->Add(dump_data_lbl, 0, wxEXPAND);
		
			std::string dump_data_str = DescribeDumpData(m_dump.get());
			wxTextCtrl* dump_data = new wxTextCtrl(
			    this, -1, dump_data_str.c_str(), wxDefaultPosition, wxDefaultSize,
			    wxTE_MULTILINE);
			dump_data->Enable(false);
			dump_data_sizer->Add(dump_data, 1, wxEXPAND);

		details_sizer->AddSpacer(32);

		wxBoxSizer* input_sizer = new wxBoxSizer(wxVERTICAL);
		details_sizer->Add(input_sizer, 1, wxEXPAND);

			input_sizer->SetMinSize(600, 400);
		
			wxString comments_description_text = _(
			    "Optional comments. Please describe (in English) what you were "
			    "doing when the problem happened. Note that these comments will be "
			    "public."
			);
			wxStaticText* comments_description_lbl = new wxStaticText(
			    this, -1, comments_description_text);
			input_sizer->Add(comments_description_lbl, 0, wxEXPAND);
			m_labels_to_reflow[comments_description_lbl] = comments_description_text;
		
			m_comments = new wxTextCtrl(
			    this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
			input_sizer->Add(m_comments, 1, wxEXPAND);
		
			input_sizer->AddSpacer(16);
		
			wxCheckBox* email_checkbox = new wxCheckBox(
			    this, -1, _("I authorize Dolphin developers to contact me about "
			                "this report."));
			input_sizer->Add(email_checkbox, 0, wxEXPAND);
		
			m_email = new wxTextCtrl(this, -1, "");
			m_email->SetHint(_("Email address"));
			m_email->Enable(false);
			input_sizer->Add(m_email, 0, wxEXPAND);

		details_sizer->AddSpacer(5);

	toplevel_sizer->AddSpacer(16);

	wxBoxSizer* controls_sizer = new wxBoxSizer(wxHORIZONTAL);
	toplevel_sizer->Add(controls_sizer, 0, wxEXPAND);

		m_progress_bar = new wxGauge(
		    this, -1, 100, wxDefaultPosition, wxDefaultSize, wxGA_SMOOTH);
		controls_sizer->Add(m_progress_bar, 1, wxEXPAND);

		wxButton* send_button = new wxButton(this, -1, _("Send"));
		controls_sizer->Add(send_button, 0, wxEXPAND);

		wxButton* cancel_button = new wxButton(this, -1, _("Cancel"));
		controls_sizer->Add(cancel_button, 0, wxEXPAND);

	toplevel_sizer->AddSpacer(5);

	cancel_button->Bind(
			wxEVT_BUTTON, &CrashReportDialog::HandleCancelEvent, this);
	email_checkbox->Bind(
	    wxEVT_CHECKBOX, &CrashReportDialog::HandleEmailCheckboxEvent, this);
	Bind(wxEVT_SIZE, &CrashReportDialog::HandleSizeEvent, this);

	SetSizerAndFit(toplevel_sizer);
}

CrashReportDialog::~CrashReportDialog()
{
}

void CrashReportDialog::HandleCancelEvent(wxCommandEvent& event)
{
	Close();
}

void CrashReportDialog::HandleEmailCheckboxEvent(wxCommandEvent& event)
{
	m_email->Enable(event.IsChecked());
}

void CrashReportDialog::HandleSizeEvent(wxSizeEvent& event)
{
	Layout();
	for (const auto& label_text_pair : m_labels_to_reflow)
	{
		auto* label_ptr = label_text_pair.first;
		const auto& text = label_text_pair.second;

		int w, h;
		label_ptr->GetSize(&w, &h);

		// The Wrap() function in wxStaticText will never grow the width of a
		// label, only shrink it. This is because it will insert \n characters in
		// the stored text. Re-insert the original text before wrapping.
		label_ptr->SetLabel(text);
		label_ptr->Wrap(w);
	}
}
