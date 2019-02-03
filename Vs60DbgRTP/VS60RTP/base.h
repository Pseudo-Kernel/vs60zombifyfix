
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

#ifndef _BASE_H_
#define	_BASE_H_


#define	__STRINGIZE(_s)		#_s
#define	_STRINGIZE(_s)		__STRINGIZE(_s)
#define	STRINGIZE(_s)		_STRINGIZE(_s)

#define	DECLARE_STUB_HANDLER(_rettype, _proc, ...)	\
	static PJMPREL32 DPStub_##_proc;					\
	_rettype WINAPI	DPHandler_##_proc(__VA_ARGS__)

#define	ORIGINAL_STUB(_proc, _type)		((_type)((DPStub_##_proc)->Stub))


#define	INITIALIZE_STUB(_libstr, _proc)	\
{	\
	PVOID _##_proc = (PVOID)GetProcAddress(LoadLibrary(TEXT(_libstr)), STRINGIZE(_proc));	\
	DPStub_##_proc = PatchInsert((ULONG_PTR)_##_proc, (ULONG_PTR)DPHandler_##_proc);	\
	DTRACE(_STRINGIZE(_proc) ": 0x%p -> 0x%p", _##_proc, DPStub_##_proc);	\
}

#define	CLEANUP_STUB(_proc)	\
{	\
	if(DPStub_##_proc) {	\
		PatchRemove(DPStub_##_proc);	\
	}	\
}


#endif
