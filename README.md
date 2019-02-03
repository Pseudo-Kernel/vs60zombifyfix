
## The VS60 debugger issue
If you press Shift+F5 to stop debugging when you're in debugging with VS60,
the debuggee becomes zombie so it can't be terminated (until you terminate VS60).

This patch fixes this issue so you can recompile happily without the error like "cannot open test.exe for writing".

Tested on Windows 7 x64.

---

## How to use

To compile, you need Visual Studio 2010 or higher version.


---
