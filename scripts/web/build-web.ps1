# [Claude 2026-10-31] B-06 Web demo (Windows PowerShell): the same build as build-web.sh (plugin/tests/web/build.mjs,
# then scripts/web/bundle.mjs), for Build-All.ps1 and a desk.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\web\build-web.ps1 [-Test] [-DryRun]
#
# Emscripten: an activated SDK (EMSDK set), else WLA_EMSDK_ENV (an emsdk_env.ps1 to dot-source), else the pinned emsdk
# in .tools\. Wonderlab: WONDERLAB_DIR, else the pinned commit cloned into .tools\. ninja: PATH, else .tools\.
param([switch]$Test, [switch]$DryRun)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
Start-Step 'B-06' ('Web demo' + $(if ($Test) { ' (test build)' } else { '' })) -DryRun:$DryRun
Set-Location $Root

if (-not $env:WONDERLAB_DIR) {
    $env:WONDERLAB_DIR = Get-Tool wonderlab
    if (-not $env:WONDERLAB_DIR) { Fail-Step 'Wonderlab is not available: set WONDERLAB_DIR to a checkout (it is a private repo)' }
}
if (-not $env:EMSDK) {
    $envScript = $env:WLA_EMSDK_ENV
    if (-not $envScript) {
        $sdk = Get-Tool emsdk
        if (-not $sdk) { Fail-Step 'Emscripten SDK could not be installed into .tools (or set EMSDK / WLA_EMSDK_ENV)' }
        $envScript = Join-Path $sdk 'emsdk_env.ps1'
    }
    # emsdk_env prints its notes on stderr; under 'Stop' Windows PowerShell would turn the first one into an error
    $env:EMSDK_QUIET = '1'
    $ErrorActionPreference = 'Continue'
    . $envScript *> $null
    $ErrorActionPreference = 'Stop'
    if (-not $env:EMSDK) { Fail-Step "could not activate Emscripten from $envScript" }
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $ninja = Get-Tool ninja
    if (-not $ninja) { Fail-Step 'ninja is not on PATH and could not be fetched' }
    $env:PATH = (Split-Path $ninja -Parent) + ';' + $env:PATH
}
New-Item -ItemType Directory -Force (Join-Path $Root 'plugin\tests\web\out') | Out-Null
$a = @('plugin/tests/web/build.mjs')
if (-not $Test) { $a += '--no-inspector' }
$code = Invoke-Logged 'node' $a
Copy-Item $script:StepLog (Join-Path $Root 'plugin\tests\web\out\build.log') -Force
if ($code) { Fail-Step 'web build failed (see the log)' }
if (-not $Test) {
    if (Invoke-Logged 'node' @('scripts/web/bundle.mjs')) { Fail-Step 'collecting build\web-bundle failed' }
    Add-Artifact (Join-Path $Root 'build\web-bundle\build-manifest.sha256')
}
if ($DryRun) { Complete-Step SKIP 'dry run: commands printed, nothing built' }
$info = Get-Content (Join-Path $Root 'build\web\test-build.json') -Raw | ConvertFrom-Json
Complete-Step PASS ("build\web, build id $($info.buildId), Wonderlab $($info.wonderlab.Substring(0, 7))" + $(if ($Test) { ', with test inspector' } else { ', bundle in build\web-bundle' }))
