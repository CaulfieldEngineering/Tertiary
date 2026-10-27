# run_all.ps1
# Written by Claude (2026-10-27). Joe: "Can we build some kind of automated test harness? So I don't have to do
# manual testing?" One command on the mini PC: build, pluginval, the host test, the unit tests and the renders,
# then one summary.
#
#   powershell -ExecutionPolicy Bypass -File plugin\tests\run_all.ps1
#   options:  -BuildDir <folder>      default build-tests (next to plugin\)
#             -JuceDir <folder>       a JUCE checkout to use instead of downloading it (e.g. ..\Tertiary\libs\juce)
#             -Reference <folder>     renders to compare with (e.g. from the last release); skipped if not given
#             -SkipBuild              use what is already built
#
# Nothing is installed: the plugin isn't copied into the system VST3 folder, and pluginval and the host test
# load it from the build folder. Nothing goes on the network except CMake fetching JUCE (unless -JuceDir).

param(
    [string]$BuildDir = "build-tests",
    [string]$JuceDir = "",
    [string]$Reference = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path "$PSScriptRoot\..\..").Path
Set-Location $Root
$Results = Join-Path $BuildDir "results"
New-Item -ItemType Directory -Force $Results | Out-Null
$Rows = @()
$Start = Get-Date

function Record([string]$Name, [bool]$Ok, [string]$Detail, [datetime]$Since) {
    $secs = [math]::Round(((Get-Date) - $Since).TotalSeconds)
    $script:Rows += [pscustomobject]@{ Check = $Name; Result = $(if ($Ok) { "PASS" } else { "FAIL" }); Seconds = $secs; Detail = $Detail }
    Write-Host ("{0,-5} {1}  ({2} s)  {3}" -f $(if ($Ok) { "PASS" } else { "FAIL" }), $Name, $secs, $Detail)
}

function Run([string]$Name, [string]$Exe, [string[]]$Arguments, [string]$Log) {
    $t = Get-Date
    # Start-Process -Wait: pluginval is a Windows GUI program, which PowerShell's & doesn't wait for
    $quoted = $Arguments | ForEach-Object { if ($_ -match ' ') { "`"$_`"" } else { $_ } }
    $options = @{ FilePath = $Exe; NoNewWindow = $true; Wait = $true; PassThru = $true
                  RedirectStandardOutput = (Join-Path $Results $Log); RedirectStandardError = (Join-Path $Results "$Log.err") }
    if ($quoted) { $options.ArgumentList = $quoted }   # Start-Process refuses an empty argument list
    $p = Start-Process @options
    $code = $p.ExitCode
    $last = (Get-Content (Join-Path $Results $Log) -Tail 1)
    Record $Name ($code -eq 0) "$last" $t
}

$Art = Join-Path $BuildDir "plugin"
$Vst3 = "$Art\Tertiary_artefacts\Release\VST3\Tertiary.vst3"

# 1. Build
if (-not $SkipBuild) {
    $t = Get-Date
    $config = @("-S", ".", "-B", $BuildDir, "-A", "x64", "-DWLA_TESTS=ON", "-DWLA_REVIEW_RENDER=ON", "-DWLA_COPY_PLUGIN_AFTER_BUILD=OFF")
    if ($JuceDir) { $config += "-DCPM_JUCE_SOURCE=$((Resolve-Path $JuceDir).Path -replace '\\','/')" }
    & cmake @config *> (Join-Path $Results "configure.log")
    & cmake --build $BuildDir --config Release --target Tertiary_VST3 HostTest TertiaryTests ReviewRender -- -m *> (Join-Path $Results "build.log")
    Record "build (VST3, HostTest, unit tests, ReviewRender)" ($LASTEXITCODE -eq 0) "log: $Results\build.log" $t
}

# 2. pluginval (downloaded once into the build folder)
$Pv = Join-Path $BuildDir "pluginval\pluginval.exe"
if (-not (Test-Path $Pv)) {
    $zip = Join-Path $BuildDir "pluginval.zip"
    Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip" -OutFile $zip
    Expand-Archive $zip -DestinationPath (Join-Path $BuildDir "pluginval") -Force
}
Run "pluginval, strictness 10" $Pv @("--strictness-level", "10", "--validate-in-process", "--validate", $Vst3) "pluginval.log"

# 3. The host test (loads the built VST3 as a DAW does)
Run "host test (VST3)" "$Art\HostTest_artefacts\Release\HostTest.exe" @("--vst3", $Vst3) "hosttest.log"

# 4. Unit tests
Run "unit tests" "$Art\TertiaryTests_artefacts\Release\TertiaryTests.exe" @() "unit-tests.log"

# 5. Renders (and a comparison, if a reference folder was given)
$py = (Get-Command python -ErrorAction SilentlyContinue).Source
Run "renders" $py @("plugin\tests\regression\render_matrix.py", "$Art\ReviewRender_artefacts\Release\ReviewRender.exe", "plugin\sample_audio", "$Results\renders") "renders.log"
if ($Reference) {
    Run "renders compared with $Reference" $py @("plugin\tests\regression\compare_renders.py", $Reference, "$Results\renders", "--tolerance", "1e-6") "compare.log"
}

# Summary
Write-Host ""
Write-Host "=== Tertiary test summary ($([math]::Round(((Get-Date) - $Start).TotalMinutes, 1)) min) ==="
$Rows | Format-Table -AutoSize | Out-String | Write-Host
$failed = @($Rows | Where-Object Result -eq "FAIL").Count
Write-Host $(if ($failed -eq 0) { "ALL PASSED" } else { "$failed FAILED - logs in $Results" })
exit $failed
