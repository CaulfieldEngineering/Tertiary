# [Codex 2026-10-30] Independent release-check entry point; shared JSON contract.
param([string]$ResultsDir = '', [string]$BuildDir = '')
$ErrorActionPreference = 'Stop'
$TestRoot = Split-Path $PSScriptRoot -Parent
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }
$Options = @('all')
if ($ResultsDir) { $Options += @('--results-dir', $ResultsDir) }
if ($BuildDir) { $Options += @('--build-dir', $BuildDir) }
& $Python (Join-Path $TestRoot 'lib/run.py') @Options
exit $LASTEXITCODE
