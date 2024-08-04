@echo off
REM ================================================================================================
REM runlocalwin.bat
REM ================================================================================================

@echo off
:: Check for permissions
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"

:: If the error flag set, we do not have admin.
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

REM Check if running on Windows
REM ================================================================================================
ver | findstr /i "windows" >nul
if %errorlevel% neq 0 (
    echo This script is for Windows only.
    pause
    exit /b 1
)

REM Navigate to correct folder
REM ================================================================================================
cd /d "%~dp0"
cd ..

REM Gather project variables from config.cmake
REM ================================================================================================
echo Reading configuration from config.cmake...

setlocal enabledelayedexpansion
for /f "tokens=1,2 delims=()" %%i in ('findstr "set(w" config.cmake ^| sed "s/set(//" ^| sed "s/)//"') do (
    set key=%%i
    set value=%%j
    set value=!value:"=!
    set !key!=!value!
    echo %%i=!value!
)

echo Variable import complete.

REM Set variables from config.cmake
REM ================================================================================================
echo Applying project variables...

set REPOSITORY_NAME=%wPLUGIN_NAME%
set FORMATTED_COMPANY_NAME=%wCOMPANY_NAME%
set FORMATTED_COMPANY_NAME=%FORMATTED_COMPANY_NAME: =%
set VERSION=%wPROJECT_VERSION%

echo Applying project variables complete.

REM Define the build type
REM ================================================================================================
set BUILD_TYPE=Release

REM Create and navigate to the build directory
REM ================================================================================================
echo Creating build folder.

mkdir build\output
cd build

echo Created build folder.

REM Configure the project
REM ================================================================================================
echo Configuring project.

cmake -S .. -B . -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if %errorlevel% neq 0 (
    echo Configuration failed.
    pause
    exit /b %errorlevel%
)

echo Configuration completed successfully.

REM Build the project
REM ================================================================================================
echo Building project.

cmake --build . --config %BUILD_TYPE%

if %errorlevel% neq 0 (
    echo Build failed.
    pause
    exit /b %errorlevel%
)

echo Build completed successfully.

REM Install Inno Setup (Windows Only)
REM ================================================================================================
echo Installing Inno Setup...

choco install innosetup --yes
if %errorlevel% neq 0 (
    echo Inno Setup installation failed.
    pause
    exit /b %errorlevel%
)

echo Inno Setup installed successfully.

REM Compile Inno Setup script (Windows Only)
REM ================================================================================================
echo Compiling Inno Setup script...

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "%cd%\installer_win.iss"
if %errorlevel% neq 0 (
    echo Inno Setup script compilation failed.
    pause
    exit /b %errorlevel%
)

echo Inno Setup script compiled successfully.

REM Pause to prevent window from closing
REM ================================================================================================
pause
