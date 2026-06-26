@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
del *.obj *.dll *.exp *.lib 2>nul
cl /EHsc /MD /O2 /W0 /D "UNICODE" cheat.cpp /link /DLL /OUT:"wt_tool.dll" user32.lib
if %ERRORLEVEL% EQU 0 (echo [+] SUCCESS - wt_tool.dll) else (echo [!] FAILED)
pause