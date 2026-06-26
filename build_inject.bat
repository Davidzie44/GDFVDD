@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /EHsc inject.cpp /Fe:inject.exe
if %ERRORLEVEL% EQU 0 (echo [+] SUCCESS - inject.exe) else (echo [!] FAILED)
pause
