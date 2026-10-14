@echo off
rem Run in an x64 Visual Studio Developer Command Prompt after building Debug.
pushd "%~dp0..\.."
cl /nologo /std:c++20 /EHsc /MDd /Od /Z7 /utf-8 /D_DEBUG /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /DENGINE_PLATFORM_WINDOWS /DENGINE_DEBUG /ISource/Runtime /ISource/ThirdParty/json /ISource/ThirdParty/stb Tests/Render/LightingIntegrationTests.cpp /FoBuild/LightingIntegrationTests.obj /FeBuild/LightingIntegrationTests.exe /link Build/Bin/Debug-windows-x86_64/HitoriEngine.lib d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib user32.lib gdi32.lib winmm.lib /DEBUG:NONE /INCREMENTAL:NO
if errorlevel 1 goto failed
Build\LightingIntegrationTests.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
