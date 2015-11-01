// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

class CrashDump
{
public:
	CrashDump();
	~CrashDump();

	// Tries to find an existing crash dump on disk and load it. Returns a null
	// pointer if no data is present on disk or if it cannot be loaded.
	static std::unique_ptr<CrashDump> TryLoadFromDisk();

	// Deletes the crash dump on disk if it exists.
	static void DeleteFromDisk();

	// Adds environment information to the dump object. Environment information
	// consists of elements that don't vary (or vary very little) across restarts
	// of the software. This would include for example OS information, drivers
	// information, locale, relevant system wide settings, etc.
	//
	// This data is not usually included in the crash dump in order to make the
	// crash dumping process simpler.
	void FillEnvironmentInformation();

	// Returns the approximate size in bytes of the crash dump. The reason why
	// this size is not exact is because it is computed before environment
	// information and additional user input are added into the dump. Given that
	// the environment info and user input are quite small compared to other
	// elements, it should still be mostly accurate. An example usage of this is
	// to show users how much data will be uploaded to the internet.
	u64 ApproximateSizeInBytes() const;

	// Serializes the crash dump to a format fit for uploading to a remote
	// server.
	std::string SerializeToString() const;

private:
};
