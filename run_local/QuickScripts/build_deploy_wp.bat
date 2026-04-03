@echo off
setlocal enabledelayedexpansion

REM =========================================================================
REM  Build Web + Deploy to WonderlandAudio.com (WordPress)
REM  Builds the WASM plugin and uploads to WordPress via SFTP.
REM  Uses SSH key auth — no password prompts.
REM =========================================================================

:: --- Elevate to Administrator ---
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

:: ---- Read config.cmake ----
for /f "tokens=*" %%a in ('findstr /c:"wPLUGIN_NAME" config.cmake') do set "LINE=%%a"
for %%w in (!LINE!) do set "LAST=%%w"
set "REPO=!LAST:)=!"
set "REPO=!REPO:"=!"

for /f "tokens=*" %%a in ('findstr /c:"wPROJECT_VERSION" config.cmake') do set "VLINE=%%a"
for %%w in (!VLINE!) do set "VLAST=%%w"
set "VERSION=!VLAST:)=!"
set "VERSION=!VERSION:"=!"

set "PLUGIN_SRC=%cd%\plugin\source"
set "WONDERLAB=%cd%\..\Wonderlab"
set "WEB_BUILD_DIR=%cd%\build\web"
set "EMSDK=C:\Users\jpcfo\emsdk"
set "SSH_KEY=C:\Users\jpcfo\.ssh\id_wp"
set "SFTP_HOST=wonderlandaudio.wordpress.com@sftp.wp.com"
set "WP_DEST=/srv/htdocs/demo/!REPO!"

echo.
echo  =========================================================================
echo    Build Web + Deploy to WonderlandAudio.com
echo  =========================================================================
echo.
echo    Plugin:   !REPO! v!VERSION!
echo    Dest:     wonderlandaudio.com/demo/!REPO!/
echo.
echo  **************************************************************************
echo  **                                                                      **
echo  **   WARNING: This will deploy to the LIVE PRODUCTION website.          **
echo  **   Changes will be immediately visible to customers.                  **
echo  **                                                                      **
echo  **   For internal testing, use build_deploy_ce.bat instead.             **
echo  **                                                                      **
echo  **************************************************************************
echo.
set /p CONFIRM="Continue with PRODUCTION deploy? (y/n): "
if /i not "!CONFIRM!"=="y" (
    echo    Cancelled.
    pause
    exit /b 0
)
echo.

:: =========================================================================
::  [1/4] Build WASM
:: =========================================================================
echo  [1/4] Building WebAssembly...
echo.

:: -- Check prerequisites --
if not exist "!EMSDK!\upstream\emscripten\emcc.bat" (
    echo    ERROR: Emscripten SDK not found at !EMSDK!
    pause
    exit /b 1
)
if not exist "!WONDERLAB!\cmake\WonderlabBuild.cmake" (
    echo    ERROR: Wonderlab not found at !WONDERLAB!
    pause
    exit /b 1
)

:: -- Find BinaryData --
set "BINARY_DATA_DIR=%cd%\build\plugin\images\juce_binarydata_%REPO%BinaryData\JuceLibraryCode"
if not exist "!BINARY_DATA_DIR!\BinaryData.h" (
    echo    BinaryData.h not found. Running native CMake configure...
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 >nul 2>&1
)
if not exist "!BINARY_DATA_DIR!\BinaryData.h" (
    echo    ERROR: BinaryData.h still not found.
    pause
    exit /b 1
)

:: -- Create build directory --
if not exist "!WEB_BUILD_DIR!" mkdir "!WEB_BUILD_DIR!"

:: -- Forward slash paths for CMake --
set "PLUGIN_SRC_FWD=!PLUGIN_SRC:\=/!"
set "WONDERLAB_FWD=!WONDERLAB:\=/!"
set "BINARY_DATA_FWD=!BINARY_DATA_DIR:\=/!"

:: -- Generate CMakeLists.txt --
echo         Generating CMakeLists.txt...
(
echo cmake_minimum_required(VERSION 3.15^)
echo project(!REPO!Web LANGUAGES C CXX^)
echo set(CMAKE_CXX_STANDARD 20^)
echo set(CMAKE_CXX_STANDARD_REQUIRED ON^)
echo.
echo set(PLUGIN_NAME "!REPO!"^)
echo set(PLUGIN_VERSION "!VERSION!"^)
echo set(PLUGIN_SRC_DIR "!PLUGIN_SRC_FWD!"^)
echo set(WONDERLAB_DIR "!WONDERLAB_FWD!"^)
echo set(PLUGIN_PROCESSOR_HEADER "PluginProcessor.h"^)
echo set(PLUGIN_PROCESSOR_CLASS audio_plugin::AudioPluginAudioProcessor^)
echo set(PLUGIN_EXTRA_INCLUDE_DIRS "!BINARY_DATA_FWD!"^)
echo set(PLUGIN_EXTRA_SOURCE_DIRS "!BINARY_DATA_FWD!"^)
echo.
echo include(!WONDERLAB_FWD!/cmake/WonderlabBuild.cmake^)
) > "!WEB_BUILD_DIR!\CMakeLists.txt"

:: -- Configure + Build --
echo         Configuring...
cd "!WEB_BUILD_DIR!"
call "!EMSDK!\upstream\emscripten\emcmake.bat" cmake -G Ninja -S . -B . -DCMAKE_BUILD_TYPE=Release >nul 2>&1
if %errorlevel% neq 0 (
    echo    ERROR: CMake configuration failed.
    pause
    exit /b 1
)

echo         Compiling...
call "!EMSDK!\upstream\emscripten\emmake.bat" ninja
if %errorlevel% neq 0 (
    echo    ERROR: Build failed.
    pause
    exit /b 1
)
cd "%~dp0\..\.."

:: -- Copy web assets --
echo         Copying web assets...
copy /y "!WONDERLAB!\web\index.html" "!WEB_BUILD_DIR!\" >nul
copy /y "!WONDERLAB!\web\index.php" "!WEB_BUILD_DIR!\" >nul
copy /y "!WONDERLAB!\web\serve.php" "!WEB_BUILD_DIR!\" >nul

:: -- Copy audio samples --
set "SAMPLE_DIR=%cd%\plugin\sample_audio"
set "TRACK_LIST="
if exist "!SAMPLE_DIR!" (
    if not exist "!WEB_BUILD_DIR!\audio" mkdir "!WEB_BUILD_DIR!\audio"
    for %%f in ("!SAMPLE_DIR!\*.*") do (
        copy "%%f" "!WEB_BUILD_DIR!\audio\" >nul
        if defined TRACK_LIST (
            set "TRACK_LIST=!TRACK_LIST!, { title: '%%~nf', file: 'audio/%%~nxf' }"
        ) else (
            set "TRACK_LIST={ title: '%%~nf', file: 'audio/%%~nxf' }"
        )
    )
)

:: -- Generate plugin-config.js --
(
echo window.WONDERLAB_CONFIG = {
echo     pluginName: '!REPO!',
echo     pluginWidth: 750,
echo     pluginHeight: 515,
echo     wasmJs: '!REPO!Web.js',
echo     tracks: [!TRACK_LIST!]
echo };
) > "!WEB_BUILD_DIR!\plugin-config.js"

echo.
echo    Build complete.

:: =========================================================================
::  [2/4] Upload web files
:: =========================================================================
echo.
echo  [2/4] Uploading web files...

:: -- Build SFTP commands file --
set "CMDS=%TEMP%\wp_deploy_cmds.txt"
(
echo cd !WP_DEST!
echo put "!WEB_BUILD_DIR!\index.html"
echo put "!WEB_BUILD_DIR!\index.php"
echo put "!WEB_BUILD_DIR!\serve.php"
echo put "!WEB_BUILD_DIR!\plugin-config.js"
echo put "!WEB_BUILD_DIR!\!REPO!Web.js"
echo put "!WEB_BUILD_DIR!\!REPO!Web.wasm"
) > "!CMDS!"

sftp -i "!SSH_KEY!" -b "!CMDS!" !SFTP_HOST!
if %errorlevel% neq 0 (
    echo    ERROR: SFTP upload failed.
    del "!CMDS!" 2>nul
    pause
    exit /b 1
)
del "!CMDS!" 2>nul

:: =========================================================================
::  [3/4] Upload audio samples
:: =========================================================================
echo.
echo  [3/4] Uploading audio...

if exist "!WEB_BUILD_DIR!\audio" (
    set "ACMDS=%TEMP%\wp_deploy_audio.txt"
    (
    echo cd !WP_DEST!/audio
    for %%f in ("!WEB_BUILD_DIR!\audio\*.*") do (
        echo put "%%f"
    )
    ) > "!ACMDS!"
    sftp -i "!SSH_KEY!" -b "!ACMDS!" !SFTP_HOST!
    del "!ACMDS!" 2>nul
) else (
    echo    No audio files, skipping.
)

:: =========================================================================
::  [4/4] Done
:: =========================================================================
echo.
echo  =========================================================================
echo    Deployed! https://wonderlandaudio.com/demo/!REPO!/
echo  =========================================================================
echo.
pause
