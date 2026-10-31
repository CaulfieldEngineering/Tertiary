# [Claude 2026-10-31] Live test dashboard for this repo: http://127.0.0.1:4120/ (see tests/README.md).
# Pass -BindHost <this machine's tailnet address> to watch it from another of Joe's machines; never a public address.
param([string]$BindHost = '127.0.0.1', [int]$Port = 4120, [switch]$Open)
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }
if ($Open) { Start-Process "http://${BindHost}:$Port/" }
& $Python (Join-Path $PSScriptRoot 'server.py') --host $BindHost --port $Port @args
