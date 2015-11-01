// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <wx/dialog.h>
#include <wx/string.h>

class CrashDump;
class wxCommandEvent;
class wxGauge;
class wxSizeEvent;
class wxStaticText;
class wxTextCtrl;

class CrashReportDialog : public wxDialog
{
public:
	CrashReportDialog(wxWindow* parent, std::unique_ptr<CrashDump> dump);
	~CrashReportDialog() override;

private:
	void HandleCancelEvent(wxCommandEvent& event);
	void HandleEmailCheckboxEvent(wxCommandEvent& event);
	void HandleSizeEvent(wxSizeEvent& event);

	std::unique_ptr<CrashDump> m_dump;

	// Labels that need to be reflowed to their new size when the dialog is
	// resized. Wx doesn't do that on its own.
	std::map<wxStaticText*, wxString> m_labels_to_reflow;

	wxTextCtrl* m_comments;
	wxTextCtrl* m_email;
	wxGauge* m_progress_bar;
};
