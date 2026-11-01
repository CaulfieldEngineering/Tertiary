# [Claude 2026-10-31] B-03 Installer (Windows): compile <build>\installer_win.iss (written by CMake from
# package_config\installer_win.iss.in) with Inno Setup into <build>\<Name>WinInstaller_<version>.exe.
# Inno Setup comes from scripts/lib/tools.json (6.7.4, into .tools\, per user, no admin); WLA_ISCC overrides it.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-Installer.ps1 [-BuildDir build] [-DryRun]
param([string]$BuildDir = 'build', [switch]$DryRun)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
Start-Step 'B-03' 'Installer' -DryRun:$DryRun

$dir = Resolve-InRepo $BuildDir
$iss = Join-Path $dir 'installer_win.iss'
if (-not (Test-Path $iss) -and -not $DryRun) { Fail-Step "$iss not found; run Build-Plugin.ps1 first" }
$iscc = Get-Tool innosetup
if (-not $iscc) { Fail-Step 'Inno Setup could not be found or downloaded (set WLA_ISCC to ISCC.exe)' }
$out = Join-Path $dir ("$($Cfg['wPLUGIN_NAME'])WinInstaller_$($Cfg['wPROJECT_VERSION']).exe")
if (Invoke-Logged $iscc @($iss)) { Fail-Step 'Inno Setup compile failed' }
if ($DryRun) { Complete-Step SKIP "dry run: would compile $iss with $iscc" }
if (-not (Test-Path $out) -or (Get-Item $out).Length -eq 0) { Fail-Step "installer not produced: $out" }
Complete-Step PASS ("$(Split-Path $out -Leaf), $([math]::Round((Get-Item $out).Length / 1MB, 1)) MB, Inno Setup " + (Split-Path (Split-Path $iscc -Parent) -Leaf))
