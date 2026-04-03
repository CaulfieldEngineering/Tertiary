@echo off
REM =========================================================================
REM  Wonderland Audio - Local Build Script (Windows)
REM
REM  Builds the native plugin (VST3 + Standalone + Installer) and optionally
REM  the Wonderlab web version (WASM + web host).
REM
REM  Prerequisites:
REM    - CMake, Visual Studio Build Tools, Chocolatey (native build)
REM    - Emscripten SDK: set EMSDK env var, or install to %USERPROFILE%\emsdk
REM    - Wonderlab repo cloned alongside this repo (web build)
REM =========================================================================

:: --- Elevate to Administrator ------------------------------------------------
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if '%errorlevel%' NEQ '0' (
    echo Requesting administrative privileges...
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
    pushd "%CD%"
    CD /D "%~dp0"

setlocal enabledelayedexpansion

:: --- Platform Check ----------------------------------------------------------
ver | findstr /i "windows" >nul
if %errorlevel% neq 0 (
    echo This script is for Windows only.
    pause
    exit /b 1
)

:: --- Navigate to Project Root ------------------------------------------------
cd /d "%~dp0"
cd ..

echo.
echo  =========================================================================
echo    Wonderland Audio - Local Build
echo  =========================================================================
echo.

:: =============================================================================
:: STEP 1: Read Project Configuration
:: =============================================================================

echo  [1/6] Reading configuration from config.cmake...
echo.

for /f "usebackq tokens=*" %%L in (`findstr "set(w" config.cmake`) do (
    set "_line=%%L"
    set "_line=!_line:set(=!"
    set "_line=!_line:)=!"
    for /f "tokens=1,*" %%a in ("!_line!") do (
        set "_key=%%a"
        set "_val=%%b"
        set "_val=!_val:"=!"
        set "_val=!_val: =!"
        set "!_key!=!_val!"
        echo         !_key! = !_val!
    )
)

set "REPOSITORY_NAME=!wPLUGIN_NAME!"
set "FORMATTED_COMPANY_NAME=!wCOMPANY_NAME!"
set "FORMATTED_COMPANY_NAME=!FORMATTED_COMPANY_NAME: =!"
set "VERSION=!wPROJECT_VERSION!"

echo.
echo         Plugin:  !REPOSITORY_NAME!
echo         Version: !VERSION!
echo         Company: !FORMATTED_COMPANY_NAME!
echo.

:: =============================================================================
:: STEP 2: Build Options
:: =============================================================================

echo  [2/6] Build options
echo.

:askVersion
set /p BUILD_VERSION="         Build Demo version? (y/n): "
if /i "%BUILD_VERSION%"=="y" (
    set DEMO_OPTION=-DBUILD_DEMO=ON
    echo         ^> Demo version selected
    echo.
) else if /i "%BUILD_VERSION%"=="n" (
    set DEMO_OPTION=-DBUILD_DEMO=OFF
    echo         ^> Full version selected
    echo.
) else (
    echo         Invalid choice. Please enter 'y' or 'n'.
    echo.
    goto askVersion
)

:askWeb
set BUILD_WEB=n
set /p BUILD_WEB="         Build Web version?  (y/n): "
if /i "%BUILD_WEB%"=="y" (
    echo         ^> Web build will run after native build
    echo         Freeing port 8080...
    for /f "tokens=5" %%p in ('netstat -ano ^| findstr ":8080.*LISTENING" 2^>nul') do (
        taskkill /F /PID %%p >nul 2>nul
    )
) else if /i "%BUILD_WEB%"=="n" (
    echo         ^> Skipping web build
) else (
    echo         Invalid choice. Please enter 'y' or 'n'.
    goto askWeb
)

echo.

:: =============================================================================
:: STEP 3: Native Build (VST3 + Standalone)
:: =============================================================================

echo  [3/6] Building native plugin...
echo.

set BUILD_TYPE=Release

:: Create build directory
mkdir build\output 2>nul
cd build

:: Configure
echo         Configuring CMake...
cmake -S .. -B . -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A x64 %DEMO_OPTION%
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: CMake configuration failed.
    pause
    exit /b %errorlevel%
)
echo         Configuration complete.
echo.

:: Build
echo         Compiling...
cmake --build . --config %BUILD_TYPE%
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: Build failed.
    pause
    exit /b %errorlevel%
)
echo         Build complete.
echo.

:: =============================================================================
:: STEP 4: Windows Installer (Inno Setup)
:: =============================================================================

echo  [4/6] Creating Windows installer...
echo.

:: Install Inno Setup if needed
echo         Checking Inno Setup...
choco install innosetup --yes
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: Inno Setup installation failed.
    pause
    exit /b %errorlevel%
)

:: Compile installer
echo         Compiling installer script...
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "%cd%\installer_win.iss"
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: Installer compilation failed.
    pause
    exit /b %errorlevel%
)
echo         Installer created successfully.
echo.

:: =============================================================================
:: STEP 5: Wonderlab Web Build (WASM)
:: =============================================================================

:: Return to project root
cd /d "%~dp0"
cd ..

if /i not "%BUILD_WEB%"=="y" goto done

echo  [5/6] Building Wonderlab web version...
echo.
echo         Plugin:  !REPOSITORY_NAME! v!VERSION!
echo.

:: -- Paths --
set "WONDERLAB_DIR=%cd%\..\Wonderlab"
set "PLUGIN_SRC_DIR=%cd%\plugin\source"
set "WEB_BUILD_DIR=%cd%\build\web"
set "BINARY_DATA_DIR=%cd%\build\plugin\images\juce_binarydata_!REPOSITORY_NAME!BinaryData\JuceLibraryCode"

:: -- Find Emscripten SDK --
if defined EMSDK (
    set "EMSDK_DIR=!EMSDK!"
) else if exist "%USERPROFILE%\emsdk" (
    set "EMSDK_DIR=%USERPROFILE%\emsdk"
) else (
    echo  ERROR: Emscripten SDK not found.
    echo         Set the EMSDK environment variable or install to %%USERPROFILE%%\emsdk
    pause
    exit /b 1
)

:: -- Validate dependencies --
if not exist "%WONDERLAB_DIR%\cmake\WonderlabBuild.cmake" (
    echo  ERROR: Wonderlab not found at %WONDERLAB_DIR%
    echo         Clone it alongside this repo first.
    pause
    exit /b 1
)
if not exist "!EMSDK_DIR!\upstream\emscripten\emcmake.bat" (
    echo  ERROR: Emscripten SDK not found at !EMSDK_DIR!
    pause
    exit /b 1
)
if not exist "!BINARY_DATA_DIR!\BinaryData.h" (
    echo  ERROR: BinaryData.h not found. Run the native build first.
    pause
    exit /b 1
)

set "EMCMAKE=!EMSDK_DIR!\upstream\emscripten\emcmake.bat"
set "EMMAKE=!EMSDK_DIR!\upstream\emscripten\emmake.bat"

:: -- Find Node.js from emsdk (discover version dynamically) --
set "NODE_EXE="
for /d %%v in ("!EMSDK_DIR!\node\*_64bit") do set "NODE_EXE=%%v\bin\node.exe"
if not defined NODE_EXE (
    echo  ERROR: Node.js not found in emsdk at !EMSDK_DIR!\node\
    pause
    exit /b 1
)

:: -- Convert paths to forward slashes for CMake --
set "PLUGIN_SRC_FWD=!PLUGIN_SRC_DIR:\=/!"
set "WONDERLAB_FWD=!WONDERLAB_DIR:\=/!"
set "BINARY_DATA_FWD=!BINARY_DATA_DIR:\=/!"

:: -- Create build directory --
mkdir "%WEB_BUILD_DIR%" 2>nul
cd "%WEB_BUILD_DIR%"

:: -- Generate CMakeLists.txt --
echo         Generating CMakeLists.txt...
(
echo cmake_minimum_required(VERSION 3.15^)
echo project(!REPOSITORY_NAME!Web LANGUAGES C CXX^)
echo set(CMAKE_CXX_STANDARD 20^)
echo set(CMAKE_CXX_STANDARD_REQUIRED ON^)
echo.
echo set(PLUGIN_NAME "!REPOSITORY_NAME!"^)
echo set(PLUGIN_VERSION "!VERSION!"^)
echo set(PLUGIN_SRC_DIR "%PLUGIN_SRC_FWD%"^)
echo set(WONDERLAB_DIR "%WONDERLAB_FWD%"^)
echo set(PLUGIN_PROCESSOR_HEADER "PluginProcessor.h"^)
echo set(PLUGIN_PROCESSOR_CLASS audio_plugin::AudioPluginAudioProcessor^)
echo set(PLUGIN_EXTRA_INCLUDE_DIRS "%BINARY_DATA_FWD%"^)
echo set(PLUGIN_EXTRA_SOURCE_DIRS "%BINARY_DATA_FWD%"^)
echo.
echo include(%WONDERLAB_FWD%/cmake/WonderlabBuild.cmake^)
) > CMakeLists.txt

:: -- Configure with Emscripten --
echo         Configuring Emscripten...
call "%EMCMAKE%" cmake -G Ninja -S . -B . -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: Emscripten configuration failed.
    pause
    exit /b %errorlevel%
)

:: -- Build WASM --
echo         Compiling WebAssembly...
call "%EMMAKE%" ninja
if %errorlevel% neq 0 (
    echo.
    echo  ERROR: WebAssembly build failed.
    pause
    exit /b %errorlevel%
)

for %%f in (!REPOSITORY_NAME!Web.wasm) do echo         WASM size: %%~zf bytes
echo.

:: =============================================================================
:: STEP 6: Assemble Web App
:: =============================================================================

:: Return to project root
cd /d "%~dp0"
cd ..

echo  [6/6] Assembling web app...
echo.

:: -- Copy Wonderlab web host files --
echo         Copying web host files...
copy "%WONDERLAB_DIR%\web\index.html" "%WEB_BUILD_DIR%\index.html" >nul

:: -- Copy sample audio tracks --
set "SAMPLE_DIR=%cd%\plugin\sample_audio"
set "TRACK_LIST="

if exist "!SAMPLE_DIR!" (
    echo         Copying sample audio tracks...
    mkdir "%WEB_BUILD_DIR%\audio" 2>nul
    for %%f in ("!SAMPLE_DIR!\*.*") do (
        copy "%%f" "%WEB_BUILD_DIR%\audio\" >nul
        if defined TRACK_LIST (
            set "TRACK_LIST=!TRACK_LIST!, { title: '%%~nf', file: 'audio/%%~nxf' }"
        ) else (
            set "TRACK_LIST={ title: '%%~nf', file: 'audio/%%~nxf' }"
        )
        echo           + %%~nxf
    )
) else (
    echo         No sample_audio folder found, skipping.
)

:: -- Generate plugin-config.js --
echo         Generating plugin-config.js...
(
echo window.WONDERLAB_CONFIG = {
echo     pluginName: '!REPOSITORY_NAME!',
echo     pluginWidth: 750,
echo     pluginHeight: 515,
echo     wasmJs: '!REPOSITORY_NAME!Web.js',
echo     tracks: [!TRACK_LIST!]
echo };
) > "%WEB_BUILD_DIR%\plugin-config.js"

echo.
echo  =========================================================================
echo    Build complete!
echo.
echo    Native:  build\plugin\%REPOSITORY_NAME%_artefacts\Release\
echo    Web:     build\web\
echo.
echo    Starting local server at http://localhost:8080
echo  =========================================================================
echo.

:: -- Kill any existing server on port 8080 --
for /f "tokens=5" %%p in ('netstat -ano ^| findstr ":8080.*LISTENING" 2^>nul') do (
    taskkill /F /PID %%p >nul 2>nul
)

:: -- Start Node.js dev server --
"!NODE_EXE!" "%WONDERLAB_DIR%\scripts\serve.js" "%WEB_BUILD_DIR%" 8080

:done
echo.
pause
