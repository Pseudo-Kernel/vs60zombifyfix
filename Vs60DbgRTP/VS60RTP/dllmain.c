
/*
 * Visual Studio 6.0 Debugger Issue Fix
 * https://bitbucket.org/Pseudo-Kernel/vs60zombifyfix
 *
 * The MIT License
 * 
 * Copyright (c) 2019, Pseudo-Kernel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software 
 * is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define	_CRT_SECURE_NO_WARNINGS
#define	_CRT_NON_CONFORMING_SWPRINTFS

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include "base.h"
#include "patch.h"
#include "handler.h"


//
// Global variables.
//

BOOL DP_IsInitialized = FALSE;
BOOL DP_TerminateProcessCalled = FALSE;
HANDLE DP_AliveMutex;
DEBUG_EVENT DP_PreviousDebugEvent;


//
// Our hook handler routines.
//

#if 0
DECLARE_STUB_HANDLER(
BOOL,
CreateProcessA,
	IN LPCSTR lpApplicationName OPTIONAL,
	IN OUT LPSTR lpCommandLine OPTIONAL,
	IN LPSECURITY_ATTRIBUTES lpProcessAttributes OPTIONAL,
	IN LPSECURITY_ATTRIBUTES lpThreadAttributes OPTIONAL,
	IN BOOL bInheritHandles,
	IN DWORD dwCreationFlags,
	IN LPVOID lpEnvironment OPTIONAL,
	IN LPCSTR lpCurrentDirectory OPTIONAL,
	IN LPSTARTUPINFOA lpStartupInfo,
	OUT LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL bResult = ORIGINAL_STUB(CreateProcessA, CREATEPROCESSA)(
		lpApplicationName, lpCommandLine,  lpProcessAttributes, 
		lpThreadAttributes, bInheritHandles, dwCreationFlags, 
		lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

	if(bResult && 
		(dwCreationFlags & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)))
	{
		DP_TerminateProcessCalled = FALSE;
	}

	return bResult;
}
#endif

DECLARE_STUB_HANDLER(
BOOL, 
TerminateProcess, 
	IN HANDLE ProcessHandle, 
	IN ULONG ExitCode)
{
	BOOL bReturn = FALSE;
	DWORD ProcessId = GetProcessId(ProcessHandle);

	DP_TerminateProcessCalled = TRUE;

	DTRACE("Entered (ProcessId = %d)", ProcessId);

	bReturn = ORIGINAL_STUB(TerminateProcess, TERMINATEPROCESS)(
		ProcessHandle, ExitCode);

	DTRACE("Leave (OriginalCall = %d)", bReturn);

	return bReturn;
}

DECLARE_STUB_HANDLER(
BOOL,
WaitForDebugEvent,
	IN LPDEBUG_EVENT lpDebugEvent,
	IN DWORD dwMilliseconds)
{
	BOOL bResult;

	//
	// In order to prevent the debugged process from being zombified,
	// we need to call DebugActiveProcessStop manually.
	//
	// [Control flow of MSDEV debugger thread]
	//    CreateProcessA(..., DEBUG_ONLY_THIS_PROCESS, ...)
	// -> WaitForDebugEvent(...) -> <Handles Debug Event> -> ContinueDebugEvent(...)
	//           |                                                    |
	//           +----<---------<---------<---------<---------<-------+
	//
	// Since the DebugActiveProcessStop call is only valid in thread 
	// which executes CreateProcess(..., DEBUG_ONLY_THIS_PROCESS, ...),
	// We must call DebugActiveProcessStop inside our WaitForDebugEvent.
	// 
	// Luckily, debugger thread polls debug event for every 50 ms
	// so we don't need to raise debug event to wake up thread from
	// sleep state.
	//
	// Now all that we need is detecting the debugging ends.
	// It can be simply done by setting termination flag to TRUE
	// inside our TerminateProcess.
	//
	
	if(DP_TerminateProcessCalled)
	{
		DebugSetProcessKillOnExit(TRUE);
		DebugActiveProcessStop(DP_PreviousDebugEvent.dwProcessId);

		DP_TerminateProcessCalled = FALSE;
	}
	
	bResult = ORIGINAL_STUB(WaitForDebugEvent, WAITFORDEBUGEVENT)(
		lpDebugEvent, dwMilliseconds);

	if(bResult)
	{
		DP_PreviousDebugEvent = *lpDebugEvent;

		DTRACE("DebugEvent %d, Thread %d, Process %d", 
			lpDebugEvent->dwDebugEventCode, 
			lpDebugEvent->dwThreadId, 
			lpDebugEvent->dwProcessId);
	}

	return bResult;
}

VOID
APIENTRY
DP_Initialize(
	VOID)
{
	if(!DP_IsInitialized)
	{
		TCHAR MutexName[100];
		HANDLE MutexHandle;

		_stprintf(MutexName, TEXT("__VS60_MSDEV_DebuggerPatcher_%d__"), GetCurrentProcessId());
		MutexHandle = CreateMutex(NULL, TRUE, MutexName);
		if(!MutexHandle)
		{
			DTRACE("Failed to create mutex, LastError = %d", GetLastError());
			return;
		}

		if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			DTRACE("Instance already initialized");
			CloseHandle(MutexHandle);
			return;
		}

		DP_AliveMutex = MutexHandle;

//		INITIALIZE_STUB("kernel32.dll", CreateProcessA);
		INITIALIZE_STUB("kernel32.dll", TerminateProcess);
		INITIALIZE_STUB("kernel32.dll", WaitForDebugEvent);

		DP_IsInitialized = TRUE;
	}
}

VOID
APIENTRY
DP_Shutdown(
	VOID)
{
	if(DP_IsInitialized)
	{
		DTRACE("Shutdowning...");

//		CLEANUP_STUB(CreateProcessA);
		CLEANUP_STUB(TerminateProcess);
		CLEANUP_STUB(WaitForDebugEvent);

		CloseHandle(DP_AliveMutex);

		DP_AliveMutex = NULL;
		DP_IsInitialized = FALSE;

		DTRACE("\n");
	}
}

BOOL
WINAPI
DllMain(
	IN HANDLE hModule, 
	IN DWORD dwReasonForCall, 
	IN LPVOID pReserved)
{
	if(dwReasonForCall == DLL_PROCESS_ATTACH)
	{
		DP_Initialize();
	}
	else if(dwReasonForCall == DLL_PROCESS_DETACH)
	{
		DP_Shutdown();
	}

	return TRUE;
}

