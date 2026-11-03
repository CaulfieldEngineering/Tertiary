# [Claude 2026-10-31] The Windows build suite: every Windows step in scripts/sequence.json, in order, each
# reported like the tests (scripts/results/<run>/progress.jsonl, <ID>.json, summary.md/json).
# Steps marked "manual" (the staging-preview deploy) never run from here.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Build-All.ps1
#   ... -NoWeb               skip the web demo
#   ... -DryRun              every step prints its commands only
#   ... -BuildDir build -Demo -ResultsDir scripts/results/my-run
param([string]$BuildDir = 'build', [switch]$Demo, [switch]$NoWeb, [switch]$DryRun, [string]$ResultsDir = '',
      [switch]$KeepGoing)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
$Started = Get-Date
if ($ResultsDir) { $env:WLA_BUILD_RESULTS = Resolve-InRepo $ResultsDir }
$ra = @()
if ($env:WLA_BUILD_RESULTS) { $ra = @('--results', $env:WLA_BUILD_RESULTS) }
$env:WLA_BUILD_RESULTS = (& $Python (Join-Path $LibDir 'report.py') run-start --run build-all @ra | Select-Object -Last 1).Trim()
$env:WLA_BUILD_NESTED = '1'
Say "Build-All: results in $env:WLA_BUILD_RESULTS"

$rows = Get-Content (Join-Path $Root 'scripts\sequence.json') -Raw | ConvertFrom-Json
$failed = $false
foreach ($row in $rows) {
    $script = $row.scripts.win
    if (-not $script -or $row.manual) { continue }
    if ($NoWeb -and $row.web) { continue }
    if ($failed -and -not $KeepGoing) {
        & $Python (Join-Path $LibDir 'report.py') begin --id $row.id --title $row.name --results $env:WLA_BUILD_RESULTS
        & $Python (Join-Path $LibDir 'report.py') end --id $row.id --status SKIP --detail 'not run: an earlier step failed' --results $env:WLA_BUILD_RESULTS | Out-Null
        continue
    }
    $a = @()
    if ($row.args -and $row.args.win) { $a += $row.args.win }
    if ($row.build_dir) { $a += @('-BuildDir', $BuildDir) }
    if ($Demo -and $row.id -eq 'B-01') { $a += '-Demo' }
    if ($DryRun) { $a += '-DryRun' }
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\$script") @a
    $code = $LASTEXITCODE
    if ($code -eq 1) { $failed = $true }
    elseif ($code -notin 0, 2, 3) {
        $failed = $true
        & $Python (Join-Path $LibDir 'report.py') end --id $row.id --status FAIL --detail "the script exited $code without a result" --results $env:WLA_BUILD_RESULTS | Out-Null
    }
}
& $Python (Join-Path $LibDir 'report.py') run-end --run build-all --results $env:WLA_BUILD_RESULTS
$code = $LASTEXITCODE
$t = (Get-Date) - $Started
Say ("Build-All started {0:yyyy-MM-dd HH:mm:ss}, ended {1:yyyy-MM-dd HH:mm:ss}, took {2} h {3} min {4} s" -f $Started, (Get-Date), [int][math]::Floor($t.TotalHours), $t.Minutes, $t.Seconds)
exit $code
