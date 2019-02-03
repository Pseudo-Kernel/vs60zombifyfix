
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

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")


// Maximum detection count.
#define	VS60_MAX_DETECTION					256

// VS60 (MSDEV.exe) detection info.
typedef struct _VS60_PROCESS_DETECTION {
	union
	{
		HWND WindowHandle;
		BOOL IsValid;
	};

	ULONG SequenceNumber;
	DWORD ProcessId;
	DWORD ThreadId;
	HANDLE ProcessHandle;
} VS60_PROCESS_DETECTION, *PVS60_PROCESS_DETECTION;

#define	VS60_PROCESS_DETECTED			1
#define	VS60_PROCESS_TERMINATED			2
#define	VS60_PROCESS_OPENED				3


// Global variables.
WCHAR VS60ModulePath_U[MAX_PATH];

VS60_PROCESS_DETECTION VS60DetectionList[VS60_MAX_DETECTION];
ULONG VS60SequenceNumber;



BOOL
VS60InsertPatch(
	IN HANDLE ProcessHandle, 
	IN PWSTR PatchModule)
{
	PVOID NameBuffer;
	ULONG BufferLength;
	ULONG BytesWritten;
	PVOID FnLoadLibraryW;
	HANDLE ThreadHandle;
	DWORD ProcessId;
	DWORD ThreadId;
	DWORD BaseAddress;

	ProcessId = GetProcessId(ProcessHandle);
	BufferLength = (wcslen(PatchModule) + 1) * sizeof(WCHAR);
	NameBuffer = VirtualAllocEx(ProcessHandle, NULL, BufferLength, MEM_COMMIT, PAGE_READWRITE);

	if(!WriteProcessMemory(ProcessHandle, NameBuffer, PatchModule, BufferLength, &BytesWritten) || 
		BytesWritten != BufferLength)
	{
		VirtualFreeEx(ProcessHandle, NameBuffer, 0, MEM_FREE);
		return FALSE;
	}

	FnLoadLibraryW = (PVOID)GetProcAddress(LoadLibrary(TEXT("kernel32.dll")), "LoadLibraryW");
	ThreadHandle = CreateRemoteThread(ProcessHandle, NULL, 0, (LPTHREAD_START_ROUTINE)FnLoadLibraryW, NameBuffer, 0, &ThreadId);

	if(!ThreadHandle)
	{
		VirtualFreeEx(ProcessHandle, NameBuffer, 0, MEM_FREE);
		return FALSE;
	}

	printf("[ %6d ] Wait for DLL attach...\n", ProcessId);
	WaitForSingleObject(ThreadHandle, INFINITE);

	BaseAddress = 0;
	GetExitCodeThread(ThreadHandle, &BaseAddress);

	VirtualFreeEx(ProcessHandle, NameBuffer, 0, MEM_FREE);
	CloseHandle(ThreadHandle);

	if(!BaseAddress)
	{
		printf("[ %6d ] Load failed!\n", ProcessId);
		return FALSE;
	}
	else
	{
		printf("[ %6d ] Loaded at base 0x%p\n", ProcessId, BaseAddress);
		return TRUE;
	}
}

VOID
VS60ListEventHandler(
	IN ULONG Event, 
	IN OUT PVS60_PROCESS_DETECTION DetectionEntry, 
	IN BOOL Opened)
{
	switch(Event)
	{
	case VS60_PROCESS_DETECTED:
		if(Opened)
		{
			printf("[ %6d ] Process detected\n", DetectionEntry->ProcessId);
			VS60InsertPatch(DetectionEntry->ProcessHandle, VS60ModulePath_U);
		}
		else
		{
			printf(
				"[ %6d ] Process detected but open failed\n"
				"If you want to apply the patch, close and reopen with administrator privilege\n", 
				DetectionEntry->ProcessId);
		}
		break;

	case VS60_PROCESS_TERMINATED:
		printf("[ %6d ] Process terminated\n", DetectionEntry->ProcessId);
		CloseHandle(DetectionEntry->ProcessHandle);

		// Zero all
		ZeroMemory(DetectionEntry, sizeof(*DetectionEntry));
		break;

	case VS60_PROCESS_OPENED:
		printf("[ %6d ] Process opened\n", DetectionEntry->ProcessId);
		VS60InsertPatch(DetectionEntry->ProcessHandle, VS60ModulePath_U);
		break;
	}
}

VOID
VS60UpdateToList(
	IN HWND hWnd, 
	IN OUT PVS60_PROCESS_DETECTION DetectionList)
{
	DWORD ProcessId = 0;
	DWORD ThreadId = 0;
	BOOL LookupSucceed = FALSE;
	INT i;

	if(hWnd)
	{
		ThreadId = GetWindowThreadProcessId(hWnd, &ProcessId);
//		printf("Detected window 0x%p - PID %d, TID %d\n", 
//			hWnd, ProcessId, ThreadId);
	}

	//
	// 1. Lookup from list.
	//

	if(hWnd)
	{
		for(i = 0; i < VS60_MAX_DETECTION; i++)
		{
			if(DetectionList[i].WindowHandle == hWnd && 
				DetectionList[i].ProcessId == ProcessId && 
				DetectionList[i].ThreadId == ThreadId)
			{
				DetectionList[i].SequenceNumber++;
				LookupSucceed = TRUE;
				break;
			}
		}
	}

	//
	// 2. Remove or reopen from list.
	//

	for(i = 0; i < VS60_MAX_DETECTION; i++)
	{
		BOOL Validated = FALSE;
		DWORD WaitResult = WAIT_FAILED;

		if(DetectionList[i].IsValid)
		{
			if(!DetectionList[i].ProcessHandle)
			{
				Validated = IsWindow(DetectionList[i].WindowHandle);

				if(Validated)
				{
					// Try to reopen
					HANDLE ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 
						DetectionList[i].ProcessId);

					if(ProcessHandle)
					{
						// Open succeed
						DetectionList[i].ProcessHandle = ProcessHandle;

						// Notify to the handler
						VS60ListEventHandler(VS60_PROCESS_OPENED, &DetectionList[i], TRUE);
					}
				}
				else
				{
					// Window is destroyed, Assume process is terminated
					VS60ListEventHandler(VS60_PROCESS_TERMINATED, &DetectionList[i], FALSE);
				}
			}
			else
			{
				WaitResult = WaitForSingleObject(DetectionList[i].ProcessHandle, 0);
				if(WaitResult == WAIT_TIMEOUT)
				{
					// Process is alive, do not notify
					Validated = TRUE;
				}
				else// if(WaitResult == WAIT_OBJECT_0)
				{
					// Process is terminated or invalid handle!
					VS60ListEventHandler(VS60_PROCESS_TERMINATED, &DetectionList[i], TRUE);
					Validated = FALSE;
				}
			}
		}
	}

	//
	// 3. Add to list if lookup failed.
	//

	if(hWnd && !LookupSucceed)
	{
		BOOL InsertSucceed = FALSE;

		for(i = 0; i < VS60_MAX_DETECTION; i++)
		{
			if(!DetectionList[i].IsValid)
			{
				HANDLE ProcessHandle;

				ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);

				DetectionList[i].WindowHandle = hWnd;
				DetectionList[i].ProcessId = ProcessId;
				DetectionList[i].ThreadId = ThreadId;
				DetectionList[i].ProcessHandle = ProcessHandle;
				DetectionList[i].SequenceNumber = VS60SequenceNumber;

				VS60ListEventHandler(VS60_PROCESS_DETECTED, &DetectionList[i], !!ProcessHandle);

				InsertSucceed = TRUE;

				// BREAK REGARDLESS OF ProcessHandle
				break;
			}
		}
	}
}

VOID
VS60TestWindow(
	IN HWND hWnd, 
	IN OUT PVS60_PROCESS_DETECTION DetectionList)
{
	TCHAR WindowName[40];
	TCHAR ClassName[40];

	WindowName[0] = ClassName[0] = 0;
	GetWindowText(hWnd, WindowName, _countof(WindowName));
	GetClassName(hWnd, ClassName, _countof(ClassName));

	if(!_tcslen(WindowName) && 
		!_tcscmp(ClassName, TEXT("MSVCDBG50")))
	{
		VS60UpdateToList(hWnd, DetectionList);
	}
}

BOOL
CALLBACK
VS60EnumChildWindowProc(
	IN HWND hWnd, 
	IN OUT PVS60_PROCESS_DETECTION DetectionList)
{
	VS60TestWindow(hWnd, DetectionList);
	return TRUE;
}

BOOL
CALLBACK
VS60EnumTopWindowProc(
	IN HWND hWnd, 
	IN OUT PVS60_PROCESS_DETECTION DetectionList)
{
	VS60TestWindow(hWnd, DetectionList);

	EnumChildWindows(hWnd, (WNDENUMPROC)VS60EnumChildWindowProc, (LPARAM)DetectionList);
	return TRUE;
}

VOID
VS60RefreshDetection(
	VOID)
{
	EnumWindows((WNDENUMPROC)VS60EnumTopWindowProc, (LPARAM)VS60DetectionList);
	VS60UpdateToList(NULL, VS60DetectionList);

	VS60SequenceNumber++;
}

DWORD
VS60WaitForProcessCreation(
	OUT PHANDLE OutProcessHandle, 
	OUT HWND *OutWindowHandle)
{
	while(1)
	{
		HWND hWnd = FindWindow(TEXT("MSVCDBG50"), NULL);
		DWORD ProcessId = 0;
		HANDLE ProcessHandle = NULL;
	
		if(!hWnd)
		{
			Sleep(1);
			continue;
		}

		GetWindowThreadProcessId(hWnd, &ProcessId);
		ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
		if(!ProcessHandle)
		{
			Sleep(1);
			continue;
		}

		*OutProcessHandle = ProcessHandle;
		*OutWindowHandle = hWnd;

		return ProcessId;
	}

	return 0;
}

VOID
VS60CreateInstance(
	VOID)
{
	HANDLE MutexHandle = CreateMutex(NULL, TRUE, TEXT("__VS60_MSDEV_Detector__"));
	if(!MutexHandle)
	{
		printf("Failed to create mutex, LastError = %d\n", GetLastError());
		ExitProcess(0);
	}

	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		printf("Instance already existing\n");
		CloseHandle(MutexHandle);
		ExitProcess(0);
	}
}

int main()
{
	HMODULE hModule;

	printf(":::::::: VS60 debugger run-time patcher ::::::::\n");
	printf(" - Last built : " __DATE__ " " __TIME__ "\n");

	SetWindowPos(GetConsoleWindow(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

	VS60CreateInstance();

	GetCurrentDirectory(MAX_PATH, VS60ModulePath_U);
	PathAppendW(VS60ModulePath_U, L"vs60rtp.dll");
	printf(" - Library Path : `%ws'\n\n", VS60ModulePath_U);


	hModule = LoadLibraryExW(VS60ModulePath_U, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if(!hModule)
	{
		printf(" !!! Failed to load library, patch may not work\n\n");
		return 0;
	}

	printf("Detecting VS60 process...\n");

	while(1)
	{
		VS60RefreshDetection();
		Sleep(200);
	}

	return 0;
}

