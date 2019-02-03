
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
#include <Windows.h>
#include "patch.h"

VOID
CDECL
DebugPrintf(
	IN PSZ Format, 
	...)
{
	CHAR Buffer[256];
	va_list args;

	va_start(args, Format);
	_vsnprintf(Buffer, sizeof(Buffer), Format, args);
	va_end(args);

	OutputDebugStringA(Buffer);
}

PJMPREL32
PatchInsert(
	IN ULONG_PTR TargetAddress, 
	IN ULONG_PTR JumpAddress)
{
	PJMPREL32 JmpRel32;
	DWORD PrevProtect;
	UCHAR NewStub[5];

	DTRACE("TargetAddress 0x%p, JumpTo 0x%p", TargetAddress, JumpAddress);
	
	JmpRel32 = (PJMPREL32)malloc(sizeof(JMPREL32));
	if(!JmpRel32)
	{
		DTRACE("Error allocating JMPREL32 struct");
		return NULL;
	}
	
	// original + stub
	JmpRel32->TargetAddress = TargetAddress;
	JmpRel32->Stub = (ULONG_PTR)VirtualAlloc(NULL, 5 + 5, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if(!JmpRel32->Stub)
	{
		DTRACE("Error allocating stub address");
		free((PVOID)JmpRel32);
		return NULL;
	}

	if(!VirtualProtect((PVOID)TargetAddress, 5, PAGE_EXECUTE_READWRITE, &PrevProtect))
	{
		DTRACE("Error unprotecting target address (0x%p)", TargetAddress);
		VirtualFree((PVOID)JmpRel32->Stub, 0, MEM_FREE);
		free((PVOID)JmpRel32);
		return NULL;
	}

	memcpy((PVOID)JmpRel32->Stub, (PVOID)TargetAddress, 5);

	DTRACE("Starting bytes: %02X %02X %02X %02X %02X ...", 
		((PUCHAR)JmpRel32->Stub)[0], 
		((PUCHAR)JmpRel32->Stub)[1], 
		((PUCHAR)JmpRel32->Stub)[2], 
		((PUCHAR)JmpRel32->Stub)[3], 
		((PUCHAR)JmpRel32->Stub)[4]);

	if(*(PUCHAR)JmpRel32->Stub == 0xe9)
	{
		DTRACE("Start with jump, Assuming default bytes...");
		memcpy((PVOID)JmpRel32->Stub, "\x8b\xff\x55\x8b\xec", 5);
	}

	if(*(PULONG)JmpRel32->Stub != 0x8b55ff8b || 
		*(PUCHAR)(JmpRel32->Stub + 4) != 0xec)
	{
		DTRACE("Starting bytes mismatch");
		VirtualFree((PVOID)JmpRel32->Stub, 0, MEM_FREE);
		free((PVOID)JmpRel32);
		return NULL;
	}

	NewStub[0] = 0xe9;
	*(PULONG)&NewStub[1] = CALC_REL32_JMP(JmpRel32->Stub + 5, TargetAddress + 5);
	memcpy((PVOID)(JmpRel32->Stub + 5), NewStub, sizeof(NewStub));
	*(PULONG)&NewStub[1] = CALC_REL32_JMP(TargetAddress, JumpAddress);
	memcpy((PVOID)TargetAddress, NewStub, sizeof(NewStub));

	DTRACE("Patch was successful.");

	return JmpRel32;
}

VOID
PatchRemove(
	IN PJMPREL32 JmpRel32)
{
	memcpy((PVOID)JmpRel32->TargetAddress, (PVOID)JmpRel32->Stub, 5);

	VirtualFree((PVOID)JmpRel32->Stub, 0, MEM_FREE);
	free((PVOID)JmpRel32);
}

