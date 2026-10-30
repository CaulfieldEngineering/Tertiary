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
#             -Asan                   [Claude 2026-10-29] also the UI tests built with the address sanitiser (6 min)
#
# [Claude 2026-10-29] Now also the UI tests (UiTest, tests\UiTest.cpp) and the standalone smoke test
# (tests\ui\smoke_standalone.py, needs pip install pywinauto). Their failures name the test-plan IDs.
#
# Nothing is installed: the plugin isn't copied into the system VST3 folder, and pluginval and the host test
# load it from the build folder. Nothing goes on the network except CMake fetching JUCE (unless -JuceDir).

param(
    [string]$BuildDir = "build-tests",
    [string]$JuceDir = "",
    [string]$Reference = "",
    [switch]$SkipBuild,
    [switch]$Asan
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
    & cmake --build $BuildDir --config Release --target Tertiary_VST3 Tertiary_Standalone HostTest TertiaryTests ReviewRender UiTest -- -m *> (Join-Path $Results "build.log")
    Record "build (VST3, standalone, HostTest, unit tests, ReviewRender, UiTest)" ($LASTEXITCODE -eq 0) "log: $Results\build.log" $t
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

# 4b. [Claude 2026-10-29] UI tests: the real editor driven by mouse and keyboard events (tests\UiTest.cpp). A failure
#     prints its failing checks here, each with its scenario and test-plan IDs and why; the full log is ui-tests.log.
function ShowFailures([string]$Log) {
    Get-Content (Join-Path $Results $Log) | Where-Object { $_ -match '^FAIL|^\s+why:|^\s+at ' } | Select-Object -First 40 | ForEach-Object { Write-Host "      $_" }
}
Run "UI tests (U-01 to U-24)" "$Art\UiTest_artefacts\Release\UiTest.exe" @("--snapshots", "$Results\ui-snapshots") "ui-tests.log"
if ($Rows[-1].Result -eq "FAIL") { ShowFailures "ui-tests.log" }

# 4c. [Claude 2026-10-29] The standalone through Windows UI Automation (tests\ui\smoke_standalone.py; needs
#     pip install pywinauto). The app's settings file is copied first and put back after: the window size and audio
#     device it remembers are untouched.
$pyw = (Get-Command python -ErrorAction SilentlyContinue).Source
& $pyw -c "import pywinauto" 2>$null
if ($LASTEXITCODE -eq 0) {
    Run "standalone smoke test (UI Automation)" $pyw @("plugin\tests\ui\smoke_standalone.py", "$Art\Tertiary_artefacts\Release\Standalone\Tertiary.exe",
        "--toggle", "Low Sync", "--appears", "Low Rate", "--slider", "Low Depth", "--menu-at", "727,25.75", "--layout", "750x478",
        "--menu-item", "About Tertiary...|Activate Tertiary...", "--dialog-button", "Activate|Deactivate this computer", "--close-button", "X",
        "--protect", "%APPDATA%\Tertiary\Tertiary.settings") "smoke.log"
    if ($Rows[-1].Result -eq "FAIL") { ShowFailures "smoke.log" }
} else {
    Record "standalone smoke test (UI Automation)" $false "pywinauto is not installed: pip install pywinauto" (Get-Date)
}

# 4d. [Claude 2026-10-29] -Asan: the UI tests built with the address sanitiser (about 6 minutes). JUCE 8.0.8 has one
#     known use-after-free of its own, in its Windows accessibility code (AccessibilityNativeHandle::get_ProviderOptions,
#     when a UI Automation client asks about a pop-up menu that has just closed). It is counted and listed, not failed.
if ($Asan) {
    $t = Get-Date
    $config = @("-S", ".", "-B", "build-asan", "-A", "x64", "-DWLA_TESTS=ON", "-DWLA_UI_TEST_ASAN=ON", "-DWLA_COPY_PLUGIN_AFTER_BUILD=OFF")
    if ($JuceDir) { $config += "-DCPM_JUCE_SOURCE=$((Resolve-Path $JuceDir).Path -replace '\\','/')" }
    & cmake @config *> (Join-Path $Results "asan-configure.log")
    & cmake --build build-asan --config Release --target UiTest -- -m *> (Join-Path $Results "asan-build.log")
    $runtime = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" | Select-Object -Last 1
    $env:PATH = "$($runtime.DirectoryName);$env:PATH"
    $env:ASAN_OPTIONS = "continue_on_error=1"
    & "build-asan\plugin\UiTest_artefacts\Release\UiTest.exe" *> (Join-Path $Results "ui-tests-asan.log")
    $code = $LASTEXITCODE
    $reports = @(Select-String -Path (Join-Path $Results "ui-tests-asan.log") -Pattern "^SUMMARY: AddressSanitizer")
    $ours = @($reports | Where-Object { $_.Line -notmatch "juce_AccessibilityElement_windows.cpp:195" })
    Record "UI tests under the address sanitiser" (($code -eq 0) -and ($ours.Count -eq 0)) "$($ours.Count) memory errors outside the known JUCE one ($($reports.Count - $ours.Count) reports of that)" $t
    $ours | Select-Object -First 10 | ForEach-Object { Write-Host "      $($_.Line)" }
}

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
