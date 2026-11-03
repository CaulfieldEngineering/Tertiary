# [Claude 2026-10-31] B-01 Plug-in (Windows): configure and build the plug-in formats from config.cmake.
# The same script runs here and on GitHub (test.yml windows, workflow.yml build). No admin rights: the build is not
# copied into the system plug-in folders unless -CopyToSystem is given.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-Plugin.ps1
#   ... -BuildDir build -Tests -ReviewRender -JuceSource C:/juce -Targets Tertiary_VST3,Tertiary_Standalone
#   ... -DryRun     prints the commands only
param(
    [string]$BuildDir = 'build',
    [string]$Config = 'Release',
    [switch]$Demo,
    [switch]$Tests,          # -DWLA_TESTS=ON (unit, host, UI and perf test programs)
    [switch]$ReviewRender,   # -DWLA_REVIEW_RENDER=ON
    [string[]]$Targets = @(),
    [string]$JuceSource = '',
    [string]$Arch = 'x64',
    [switch]$CopyToSystem,
    [string[]]$CMakeArgs = @(),
    [switch]$NoConfigure,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
Start-Step 'B-01' 'Plug-in' -DryRun:$DryRun

$name = $Cfg['wPLUGIN_NAME']
$dir = Resolve-InRepo $BuildDir
if (-not $dir.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase)) { Fail-Step "build folder $dir is outside this repo" }
$cmake = if ($env:WLA_TEST_CMAKE) { $env:WLA_TEST_CMAKE } else { 'cmake' }

if (-not $NoConfigure) {
    $a = @('-S', $Root, '-B', $dir, '-A', $Arch, "-DCMAKE_BUILD_TYPE=$Config",
           ('-DBUILD_DEMO=' + $(if ($Demo) { 'ON' } else { 'OFF' })),
           ('-DWLA_COPY_PLUGIN_AFTER_BUILD=' + $(if ($CopyToSystem) { 'ON' } else { 'OFF' })))
    if ($Tests) { $a += '-DWLA_TESTS=ON' }
    if ($ReviewRender) { $a += '-DWLA_REVIEW_RENDER=ON' }
    if ($JuceSource) { $a += "-DCPM_JUCE_SOURCE=$($JuceSource -replace '\\', '/')" }
    $a += $CMakeArgs
    if (Invoke-Logged $cmake $a) { Fail-Step 'CMake configure failed' }
}
$b = @('--build', $dir, '--config', $Config, '--parallel')
foreach ($t in ($Targets | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) { $b += @('--target', $t) }
if (Invoke-Logged $cmake $b) { Fail-Step 'build failed' }

if ($DryRun) { Complete-Step SKIP 'dry run: commands printed, nothing built' }
$art = Join-Path $dir "plugin\${name}_artefacts\$Config"
$built = @()
$wanted = @()
if ($Cfg['wFORMATS'] -match '\bVST3\b' -and (-not $Targets -or ($Targets -join ',') -match '_VST3|^$')) { $wanted += "VST3\$name.vst3" }
if ($Cfg['wFORMATS'] -match '\bStandalone\b' -and (-not $Targets -or ($Targets -join ',') -match '_Standalone')) { $wanted += "Standalone\$name.exe" }
foreach ($w in $wanted) {
    $p = Join-Path $art $w
    if (-not (Test-Path $p)) { Fail-Step "expected $p was not built" }
    $built += $w
}
Complete-Step PASS ("built $($built -join ', ') in $BuildDir ($Config" + $(if ($Demo) { ', demo' } else { '' }) + $(if ($Targets) { "; targets $($Targets -join ',')" } else { '' }) + ')')
