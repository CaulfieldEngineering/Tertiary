# [Claude 2026-10-31] Shared helpers for scripts/win/*.ps1 and scripts/web/*.ps1. Dot-source it; it sets $Root and
# $Cfg (config.cmake as a hashtable) and reports each step the way tests/ does (scripts/lib/report.py:
# progress.jsonl and <ID>.json). Windows PowerShell 5.1 or PowerShell 7. Nothing here needs admin rights.
#
#   . (Join-Path $PSScriptRoot '..\lib\common.ps1')
#   Start-Step 'B-01' 'Plug-in'
#   Invoke-Logged cmake @('--build', 'build')     # returns the exit code
#   Complete-Step PASS 'built ...'                # exits: 0 PASS, 1 FAIL, 2 SKIP, 3 PLANNED (on GitHub only FAIL is non-zero)

$LibDir = $PSScriptRoot
if (-not $LibDir) { $LibDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
$Root = (Resolve-Path (Join-Path $LibDir '..\..')).Path
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }

function Read-WConfig {
    $cfg = @{}
    foreach ($line in Get-Content (Join-Path $Root 'config.cmake')) {
        if ($line -match '^\s*set\((w[A-Za-z0-9_]+)\s+(.*)\)\s*(#.*)?$') {
            $cfg[$Matches[1]] = (($Matches[2] -replace '"', '') -replace '\s+', ' ').Trim()
        }
    }
    foreach ($k in @($cfg.Keys)) { $cfg[$k] = $cfg[$k].Replace('${wPLUGIN_NAME}', $cfg['wPLUGIN_NAME']) }
    return $cfg
}
$Cfg = Read-WConfig

$script:StepId = ''
$script:StepLog = ''
$script:StepArtifacts = @()
$script:DryRunMode = $false

function In-CI { return $env:GITHUB_ACTIONS -eq 'true' }
function On-Tag { return ($env:GITHUB_REF -like 'refs/tags/v*') }
function Say($text) { Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $text) }
function Warn($text) { if (In-CI) { Write-Host "::warning::$text" } else { Say "WARNING: $text" } }
function Err($text) { if (In-CI) { Write-Host "::error::$text" } else { Say "ERROR: $text" } }

function Start-Step([string]$Id, [string]$Title, [switch]$DryRun) {
    $script:StepId = $Id
    $script:DryRunMode = [bool]$DryRun
    if (-not $env:WLA_BUILD_RESULTS) {
        $env:WLA_BUILD_RESULTS = (& $Python (Join-Path $LibDir 'report.py') folder | Select-Object -Last 1).Trim()
    }
    $art = Join-Path $env:WLA_BUILD_RESULTS "$Id-artifacts"
    New-Item -ItemType Directory -Force $art | Out-Null
    $script:StepLog = Join-Path $art ($Id.ToLower() + '.log')
    Set-Content -Path $script:StepLog -Value '' -Encoding utf8
    $script:StepArtifacts = @($script:StepLog)
    & $Python (Join-Path $LibDir 'report.py') begin --id $Id --title $Title --results $env:WLA_BUILD_RESULTS
    Say ("$Id $Title" + $(if ($DryRun) { ' (dry run)' } else { '' }))
}

function Add-Artifact([string]$Path) { $script:StepArtifacts += $Path }

function Complete-Step([ValidateSet('PASS', 'FAIL', 'SKIP', 'PLANNED')][string]$Status, [string]$Detail) {
    $a = @()
    foreach ($p in $script:StepArtifacts) { if (Test-Path $p) { $a += @('--artifact', $p) } }
    & $Python (Join-Path $LibDir 'report.py') end --id $script:StepId --status $Status --detail $Detail --results $env:WLA_BUILD_RESULTS @a
    $code = @{ PASS = 0; FAIL = 1; SKIP = 2; PLANNED = 3 }[$Status]
    if ((In-CI) -and $code -gt 1) { $code = 0 }
    exit $code
}

function Fail-Step([string]$Detail) { Err $Detail; Complete-Step FAIL $Detail }

# Runs a native command with its output on the console and in the step log; returns its exit code.
# In a dry run it only prints the command.
function Invoke-Logged([string]$Exe, [string[]]$Arguments = @()) {
    $line = '> ' + $Exe + ' ' + ($Arguments -join ' ')
    Write-Host $line
    Add-Content -Path $script:StepLog -Value $line -Encoding utf8
    if ($script:DryRunMode) { return 0 }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Exe @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $script:StepLog -Append | Out-Host
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
    return $code
}

# A pinned tool from scripts/lib/tools.json (downloaded into .tools/ the first time). $null if it cannot be had.
function Get-Tool([string]$Name, [switch]$NoInstall) {
    $a = @((Join-Path $LibDir 'tools.py'), 'path', $Name)
    if ($NoInstall) { $a += '--no-install' }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $out = & $Python @a 2>&1 | ForEach-Object { "$_" } } finally { $ErrorActionPreference = $old }
    if ($LASTEXITCODE) { $out | ForEach-Object { Write-Host $_ }; return $null }
    return ($out | Where-Object { $_ -notlike '`[tools`]*' } | Select-Object -Last 1).Trim()
}

function Get-BuildSuffix([string]$BuildDir) {
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if ((Test-Path $cache) -and (Select-String -Path $cache -Pattern '^BUILD_DEMO:BOOL=ON' -Quiet)) { return '.D' }
    return ''
}

function Resolve-InRepo([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $Root $Path))
}
