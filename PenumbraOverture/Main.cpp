/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of Penumbra Overture.
 *
 * Penumbra Overture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Penumbra Overture is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Penumbra Overture.  If not, see <http://www.gnu.org/licenses/>.
 */
//#include <vld.h>

#include "Init.h"

#include "SDL/SDL.h"

#ifdef WIN32
	#include <windows.h>
#endif

//-----------------------------------------------------------------------

#ifdef WIN32

// Automatic crash reporting: when anything machine-specific (audio driver,
// GPU driver, compositor, SteamVR) takes the process down, write a minidump
// next to the executable plus one flushed line into hpl.log. Support then
// only ever needs files the game produced by itself. dbghelp.dll ships with
// every Windows installation and is resolved at runtime, so nothing new is
// linked and nothing has to be preinstalled on the player's machine.
typedef BOOL(WINAPI *MiniDumpWriteDumpFunc)(HANDLE, DWORD, HANDLE, DWORD, void *, void *, void *);

struct MiniDumpExceptionInfo
{
	DWORD mlThreadId;
	EXCEPTION_POINTERS *mpExceptionPointers;
	BOOL mbClientPointers;
};

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *apExceptionPointers)
{
	Log("CRASH: unhandled exception 0x%08X at address %p\n",
		apExceptionPointers != NULL ? apExceptionPointers->ExceptionRecord->ExceptionCode : 0,
		apExceptionPointers != NULL ? apExceptionPointers->ExceptionRecord->ExceptionAddress : NULL);

	// Address-space exhaustion is the classic silent killer of 32-bit VR
	// builds; record how much was left at the moment of death.
	MEMORYSTATUSEX vMemory;
	memset(&vMemory, 0, sizeof(vMemory));
	vMemory.dwLength = sizeof(vMemory);
	if(GlobalMemoryStatusEx(&vMemory))
	{
		Log("CRASH: %.0f MB of process address space still free\n",
			vMemory.ullAvailVirtual / (1024.0 * 1024.0));
	}

	char sExePath[MAX_PATH];
	if(GetModuleFileNameA(NULL, sExePath, MAX_PATH) == 0) return EXCEPTION_EXECUTE_HANDLER;

	std::string sDumpPath(sExePath);
	const size_t lSlash = sDumpPath.find_last_of("\\/");
	if(lSlash == std::string::npos) return EXCEPTION_EXECUTE_HANDLER;
	sDumpPath.resize(lSlash + 1);
	sDumpPath += "penumbravr_crash.dmp";

	HANDLE hFile = CreateFileA(sDumpPath.c_str(), GENERIC_WRITE, 0, NULL,
							CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		Log("CRASH: could not create '%s' (error %lu)\n", sDumpPath.c_str(), GetLastError());
		return EXCEPTION_EXECUTE_HANDLER;
	}

	HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
	bool bWritten = false;
	if(hDbgHelp != NULL)
	{
		MiniDumpWriteDumpFunc pWriteDump =
			(MiniDumpWriteDumpFunc)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
		if(pWriteDump != NULL)
		{
			MiniDumpExceptionInfo Info;
			Info.mlThreadId = GetCurrentThreadId();
			Info.mpExceptionPointers = apExceptionPointers;
			Info.mbClientPointers = FALSE;

			bWritten = pWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
								hFile, 0, &Info, NULL, NULL) != FALSE;
		}
	}
	CloseHandle(hFile);

	Log("CRASH: %s '%s'\n", bWritten ? "minidump written to" : "failed writing", sDumpPath.c_str());

	return EXCEPTION_EXECUTE_HANDLER;
}

#endif

//-----------------------------------------------------------------------

int hplMain(const tString& asCommandLine)
{
#ifdef WIN32
	SetUnhandledExceptionFilter(CrashFilter);
#endif

	cInit *pInit = hplNew( cInit, () );

	bool bRet = pInit->Init(asCommandLine);
	
	if(bRet==false){
		hplDelete( pInit->mpGame );
		CreateMessageBoxW(_W("Error!"),pInit->msErrorMessage.c_str());
		OpenBrowserWindow(_W("http://support.frictionalgames.com"));
		return 1;
	}

	pInit->Run();

	pInit->Exit();

	hplDelete( pInit );
	
	cMemoryManager::LogResults();

	return 0;
}
