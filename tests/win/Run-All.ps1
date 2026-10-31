# [Codex 2026-10-30] all: independently runnable; common result contract.
param([string]$ResultsDir = '', [string]$BuildDir = '',
      [ValidateSet('local','ci')][string]$Profile = 'local', [switch]$ExpectDemo,
      [string]$Engine = 'chromium', [switch]$Dashboard)
$ErrorActionPreference = 'Stop'
$TestRoot = Split-Path $PSScriptRoot -Parent
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }
$Options = @('all', '--profile', $Profile, '--engine', $Engine)
if ($ResultsDir) { $Options += @('--results-dir', $ResultsDir) }
if ($BuildDir) { $Options += @('--build-dir', $BuildDir) }
if ($ExpectDemo) { $Options += '--expect-demo' }
if ($Dashboard) { $Options += '--dashboard' }  # [Claude 2026-10-31] live dashboard, tests/dashboard
& $Python (Join-Path $TestRoot 'lib/run.py') @Options
exit $LASTEXITCODE
