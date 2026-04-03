@echo off
REM =========================================================================
REM  Quick Web Build + Deploy to CaulfieldEngineering.com
REM  No user input. No native installer. Just web build and deploy.
REM =========================================================================

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
cd /d "%~dp0"
cd ..\..

echo.
echo  =========================================================================
echo    Web Build + Deploy (no user input)
echo  =========================================================================
echo.

:: ==========================================================================
:: STEP 1: Read config.cmake
:: ==========================================================================
echo  [1/5] Reading config.cmake...

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
    )
)

set "REPOSITORY_NAME=!wPLUGIN_NAME!"
set "VERSION=!wPROJECT_VERSION!"
echo         Plugin: !REPOSITORY_NAME! v!VERSION!
echo.

:: ==========================================================================
:: STEP 2: Native CMake (generates BinaryData.h — required by web build)
:: ==========================================================================
echo  [2/5] Running native CMake configure (for BinaryData.h)...

set "BINARY_DATA_DIR=%cd%\build\plugin\images\juce_binarydata_!REPOSITORY_NAME!BinaryData\JuceLibraryCode"

if not exist "!BINARY_DATA_DIR!\BinaryData.h" (
    echo         BinaryData.h not found, running native configure...
    mkdir build 2>nul
    cd build
    cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -A x64 -DBUILD_DEMO=ON
    if !errorlevel! neq 0 (
        echo  ERROR: Native CMake configure failed.
        pause
        exit /b 1
    )
    cd ..
    echo         BinaryData.h generated.
) else (
    echo         BinaryData.h already exists, skipping native configure.
)
echo.

:: ==========================================================================
:: STEP 3: Web Build (Emscripten WASM)
:: ==========================================================================
echo  [3/5] Building web version...
echo.

set "WONDERLAB_DIR=%cd%\..\Wonderlab"
set "PLUGIN_SRC_DIR=%cd%\plugin\source"
set "WEB_BUILD_DIR=%cd%\build\web"
set "EMSDK_DIR=C:\Users\jpcfo\emsdk"

if not exist "%WONDERLAB_DIR%\cmake\WonderlabBuild.cmake" (
    echo  ERROR: Wonderlab not found at %WONDERLAB_DIR%
    pause
    exit /b 1
)
if not exist "%EMSDK_DIR%\upstream\emscripten\emcmake.bat" (
    echo  ERROR: Emscripten SDK not found at %EMSDK_DIR%
    pause
    exit /b 1
)

set "EMCMAKE=%EMSDK_DIR%\upstream\emscripten\emcmake.bat"
set "EMMAKE=%EMSDK_DIR%\upstream\emscripten\emmake.bat"

set "PLUGIN_SRC_FWD=!PLUGIN_SRC_DIR:\=/!"
set "WONDERLAB_FWD=!WONDERLAB_DIR:\=/!"
set "BINARY_DATA_FWD=!BINARY_DATA_DIR:\=/!"

mkdir "%WEB_BUILD_DIR%" 2>nul
cd "%WEB_BUILD_DIR%"

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

echo         Configuring Emscripten...
call "%EMCMAKE%" cmake -G Ninja -S . -B . -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo  ERROR: Emscripten configuration failed.
    pause
    exit /b 1
)

echo         Compiling WebAssembly...
call "%EMMAKE%" ninja
if %errorlevel% neq 0 (
    echo  ERROR: WebAssembly build failed.
    pause
    exit /b 1
)

for %%f in (!REPOSITORY_NAME!Web.wasm) do echo         WASM size: %%~zf bytes
echo.

:: ==========================================================================
:: STEP 4: Assemble web app
:: ==========================================================================
cd /d "%~dp0"
cd ..

echo  [4/5] Assembling web app...

copy "%WONDERLAB_DIR%\web\index.html" "%WEB_BUILD_DIR%\index.html" >nul

set "SAMPLE_DIR=%cd%\plugin\sample_audio"
set "TRACK_LIST="

if exist "!SAMPLE_DIR!" (
    echo         Copying audio samples...
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
)

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

:: ==========================================================================
:: STEP 5: Deploy to CaulfieldEngineering.com
:: ==========================================================================
echo  [5/5] Deploying to server...
echo.

set "SSH_KEY=C:\Users\jpcfo\.ssh\id_rsa"
set "SERVER=caulfiel@CaulfieldEngineering.com"
set "DEST=/home/caulfiel/public_html/plug-ins/!REPOSITORY_NAME!/web/"

echo         Creating remote directory...
ssh -p 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no %SERVER% "mkdir -p %DEST% %DEST%audio/"

echo         Uploading files...
scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%WEB_BUILD_DIR%\index.html" %SERVER%:%DEST%
scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%WEB_BUILD_DIR%\plugin-config.js" %SERVER%:%DEST%
scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%WEB_BUILD_DIR%\!REPOSITORY_NAME!Web.js" %SERVER%:%DEST%
scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%WEB_BUILD_DIR%\!REPOSITORY_NAME!Web.wasm" %SERVER%:%DEST%

if exist "%WEB_BUILD_DIR%\audio\*" (
    echo         Uploading audio samples...
    scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%WEB_BUILD_DIR%\audio\*" %SERVER%:%DEST%audio/
)

echo         Uploading .htaccess...
(
echo DirectoryIndex index.html
echo.
echo Header set Cross-Origin-Opener-Policy "same-origin"
echo Header set Cross-Origin-Embedder-Policy "credentialless"
echo Header set Cache-Control "no-cache, no-store, must-revalidate"
echo.
echo Header set X-Frame-Options "ALLOWALL"
echo Header set Content-Security-Policy "frame-ancestors *"
echo.
echo AddType application/wasm .wasm
echo AddType application/javascript .js
) > "%TEMP%\htaccess_web"
scp -P 7822 -i "%SSH_KEY%" -o StrictHostKeyChecking=no "%TEMP%\htaccess_web" %SERVER%:%DEST%.htaccess

echo.
echo  =========================================================================
echo    Done!
echo    https://caulfieldengineering.com/plug-ins/!REPOSITORY_NAME!/web/
echo  =========================================================================
echo.
pause
