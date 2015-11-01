// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/CrashDump.h"

CrashDump::CrashDump()
{
}

CrashDump::~CrashDump()
{
}

void CrashDump::FillEnvironmentInformation()
{
}

std::unique_ptr<CrashDump> CrashDump::TryLoadFromDisk()
{
	std::unique_ptr<CrashDump> dump(new CrashDump());
	dump->FillEnvironmentInformation();
	return dump;
}

void CrashDump::DeleteFromDisk()
{
}

std::string CrashDump::SerializeToString()
{
	return "Hello, I'm a fake crash dump.";
}
