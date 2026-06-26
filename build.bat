@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
del *.obj *.dll *.exp *.lib 2>nul

set IMGUI_DIR=imgui

cl /EHsc /MD /O2 /W0 /D "UNICODE" /I"%IMGUI_DIR%" ^
    cheat.cpp ^
    "%IMGUI_DIR%/imgui.cpp" ^
    "%IMGUI_DIR%/imgui_draw.cpp" ^
    "%IMGUI_DIR%/imgui_tables.cpp" ^
    "%IMGUI_DIR%/imgui_widgets.cpp" ^
    "%IMGUI_DIR%/imgui_impl_dx11.cpp" ^
    "%IMGUI_DIR%/imgui_impl_win32.cpp" ^
    /link /DLL /OUT:"wt_tool.dll" user32.lib

if %ERRORLEVEL% EQU 0 (echo [+] SUCCESS - wt_tool.dll) else (echo [!] FAILED)
pause