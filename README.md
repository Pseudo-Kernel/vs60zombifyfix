
## The VS60 debugger issue
If you press Shift+F5 to stop debugging when you're in debugging with VS60,
the debuggee becomes zombie so it can't be terminated (until you terminate VS60).

This patch fixes this issue so you can recompile happily without the error like "cannot open test.exe for writing".

Tested on Windows 7 x64 / Windows 10 x64.

---

## How to use

To compile, you need Visual Studio 2010 or higher version.

1. Open the Vs60DbgRTP.sln in Vs60DbgRTP directory.
2. Compile it.
3. You'll see VS60Detector.exe and VS60RTP.dll (in Vs60DbgRTP\Debug or Vs60DbgRTP.sln\Release)
4. Execute VS60Detector.exe. Note that VS60Detector.exe and VS60RTP.dll must be in the **same directory**.
5. The VS60Detector.exe will detect the Visual Studio 6.0 process and apply patch automatically.
6. If this program not works, close the program and try reopen with administrator privilege.

---
