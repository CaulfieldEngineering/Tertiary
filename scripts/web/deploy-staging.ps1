# [Claude 2026-10-31] B-07 Web staging preview from Windows: runs deploy-staging.sh with Git's bash (ssh/scp/sha256
# come with Git for Windows). Same rules: private preview only, clean checkout that is the current origin/staging commit.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\web\deploy-staging.ps1 -DryRun
param([switch]$DryRun)
$git = (Get-Command git -ErrorAction Stop).Source
$bash = Join-Path (Split-Path (Split-Path $git -Parent) -Parent) 'bin\bash.exe'
if (-not (Test-Path $bash)) { $bash = 'bash' }
$a = @((Join-Path $PSScriptRoot 'deploy-staging.sh'))
if ($DryRun) { $a += '--dry-run' }
& $bash @a
exit $LASTEXITCODE
