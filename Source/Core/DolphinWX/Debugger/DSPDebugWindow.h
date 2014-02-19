// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// general things
#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <wx/button.h>
#include <wx/frame.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/wx.h>
#include <wx/aui/aui.h>

#include "Core/DSP/disassemble.h"
#include "Core/DSP/DSPInterpreter.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/HW/DSPLLE/DSPDebugInterface.h"

class DSPRegisterView;
class CCodeView;
class CMemoryView;

class DSPDebuggerLLE : public wxPanel
{
public:
	DSPDebuggerLLE(wxWindow *parent, wxWindowID id = wxID_ANY);
	virtual ~DSPDebuggerLLE();

	void Update();

private:
	DECLARE_EVENT_TABLE();

	enum
	{
		ID_TOOLBAR = 1000,
		ID_RUNTOOL,
		ID_STEPTOOL,
		ID_SHOWPCTOOL,
		ID_ADDRBOX,
		ID_SYMBOLLIST,
		ID_DSP_REGS
	};

	DSPDebugInterface debug_interface;
	u64 m_CachedStepCounter;

	// GUI updaters
	void UpdateDisAsmListView();
	void UpdateRegisterFlags();
	void UpdateSymbolMap();
	void UpdateState();

	// GUI items
	wxAuiManager m_mgr;
	wxAuiToolBar* m_Toolbar;
	CCodeView* m_CodeView;
	CMemoryView* m_MemView;
	DSPRegisterView* m_Regs;
	wxListBox* m_SymbolList;
	wxAuiNotebook* m_MainNotebook;

	void OnClose(wxCloseEvent& event);
	void OnChangeState(wxCommandEvent& event);
	//void OnRightClick(wxListEvent& event);
	//void OnDoubleClick(wxListEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnSymbolListChange(wxCommandEvent& event);

	bool JumpToAddress(u16 addr);

	void FocusOnPC();
	//void UnselectAll();
};

extern DSPDebuggerLLE* m_DebuggerFrame;
