@echo off
rem Memoria by DSK - local Windows build (generic, no personal paths).
rem Usage: build_local.bat [Release^|Debug] [package]
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if "%GX_DEPS%"=="" set "GX_DEPS=%USERPROFILE%\.gx-deps"
cd /d "%~dp0"

rem Load MSVC x64 environment (VS 2022 / Build Tools) if cl.exe is not on PATH
where cl >nul 2>nul
if not errorlevel 1 goto :haveCl
rem (no "for" loop here: the "(x86)" in the vswhere path breaks for/if blocks and silently aborts cmd)
set "VSDIR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :vsFallback
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\gx_vsdir.txt" 2>nul
set /p VSDIR=<"%TEMP%\gx_vsdir.txt"
del "%TEMP%\gx_vsdir.txt" >nul 2>nul
:vsFallback
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :vsFound
set "VSDIR=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :vsFound
set "VSDIR=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :vsFound
set "VSDIR=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :vsFound
set "VSDIR=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :vsFound
echo ERROR: no se encuentra Visual Studio 2022 con C++ (Desarrollo para el escritorio con C++).
goto :fail
:vsFound
echo Visual Studio: %VSDIR%
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || goto :fail
:haveCl

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% "-DFETCHCONTENT_BASE_DIR=%GX_DEPS%" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 || goto :fail
cmake --build build || goto :fail
echo.
echo Pasando los tests (unos 40-60 segundos sin mensajes, es normal)...
ctest --test-dir build --output-on-failure || goto :fail
echo.
echo Empaquetando...

if /i not "%~2"=="package" goto :done
for /f "usebackq delims=" %%v in (`python scripts\version.py`) do set "VER=%%v"
python scripts\package.py --platform win --build build --config %CONFIG% --version %VER% || goto :fail
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" goto :noInno
"%ISCC%" /Q "/DAppVersion=%VER%" "/DArtDir=..\build\Memoria_artefacts\%CONFIG%" installer\memoria.iss || goto :fail
python scripts\package.py --platform win --build build --checksums-only || goto :fail
goto :done
:noInno
echo (Inno Setup 6 no encontrado: se omite el instalador .exe)
:done
echo.
echo VST3:       build\Memoria_artefacts\%CONFIG%\VST3\Memoria by DSK.vst3
echo Standalone: build\Memoria_artefacts\%CONFIG%\Standalone\Memoria by DSK.exe
exit /b 0
:fail
echo BUILD FAILED
exit /b 1
