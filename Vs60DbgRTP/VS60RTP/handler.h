
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


#ifndef _HANDLER_H_
#define	_HANDLER_H_


typedef
BOOL
(WINAPI *DEBUGACTIVEPROCESS)(
	IN DWORD dwProcessId);

typedef
BOOL
(WINAPI *DEBUGACTIVEPROCESSSTOP)(
	IN DWORD dwProcessId);

typedef
BOOL
(WINAPI *TERMINATEPROCESS)(
	IN HANDLE ProcessHandle, 
	IN ULONG ExitCode);

typedef
BOOL
(WINAPI *TERMINATETHREAD)(
	IN HANDLE ThreadHandle, 
	IN ULONG ExitCode);

typedef
BOOL
(WINAPI *WAITFORDEBUGEVENT)(
	IN LPDEBUG_EVENT lpDebugEvent,
	IN DWORD dwMilliseconds);

typedef
BOOL
(WINAPI *CREATEPROCESSA)(
	IN LPCSTR lpApplicationName OPTIONAL,
	IN OUT LPSTR lpCommandLine OPTIONAL,
	IN LPSECURITY_ATTRIBUTES lpProcessAttributes OPTIONAL,
	IN LPSECURITY_ATTRIBUTES lpThreadAttributes OPTIONAL,
	IN BOOL bInheritHandles,
	IN DWORD dwCreationFlags,
	IN LPVOID lpEnvironment OPTIONAL,
	IN LPCSTR lpCurrentDirectory OPTIONAL,
	IN LPSTARTUPINFOA lpStartupInfo,
	OUT LPPROCESS_INFORMATION lpProcessInformation);


#endif
