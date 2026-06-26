@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
del *.dll *.exp *.lib 2>nul

cl /EHsc /MD /O2 /W0 /D "UNICODE" /I"imgui" cheat.cpp imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_impl_dx11.obj imgui_impl_win32.obj /link /DLL /OUT:"wt_tool.dll" user32.lib d3d11.lib dxgi.lib

if %ERRORLEVEL% EQU 0 (echo [+] SUCCESS - wt_tool.dll) else (echo [!] FAILED)
pause