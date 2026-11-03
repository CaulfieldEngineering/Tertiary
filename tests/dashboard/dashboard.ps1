# [Claude 2026-10-31] Turns the live test dashboard on or off (Dashboard-On.bat / Dashboard-Off.bat).
# On: runs server.py hidden, bound to this machine's tailnet address on port 4120 (127.0.0.1 if Tailscale has
# no address), writes .server.pid here, opens the page. Already running: just opens the page.
# Off: stops the process in .server.pid, else whatever dashboard is listening on port 4120, and says which.
param([ValidateSet('on', 'off')][string]$Action = 'on', [int]$Port = 4120, [switch]$NoOpen)
$here = $PSScriptRoot
$pidFile = Join-Path $here '.server.pid'
$python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }

function Get-Running {
    if (Test-Path $pidFile) {
        $id = [int](Get-Content $pidFile -Raw).Trim()
        $p = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($p) { return $p }
    }
    return $null
}

if ($Action -eq 'on') {
    $address = $null
    try { $address = (& tailscale ip -4 2>$null | Select-Object -First 1).Trim() } catch { }
    if (-not $address) { $address = '127.0.0.1' }
    $url = "http://${address}:$Port/"
    if (Get-Running) {
        Write-Host "The dashboard is already running: $url"
    } else {
        $p = Start-Process -PassThru -WindowStyle Hidden -FilePath $python `
            -ArgumentList @("`"$(Join-Path $here 'server.py')`"", '--host', $address, '--port', $Port) -WorkingDirectory (Split-Path (Split-Path $here))
        Set-Content -Path $pidFile -Value $p.Id
        Start-Sleep -Seconds 2
        if ($p.HasExited) { Remove-Item $pidFile -ErrorAction SilentlyContinue; Write-Host "The dashboard did not start (port $Port may be in use)."; exit 1 }
        Write-Host "The dashboard is running: $url"
    }
    if (-not $NoOpen) { Start-Process $url }
    exit 0
}

$p = Get-Running
if ($p) {
    Stop-Process -Id $p.Id -Force
    Write-Host "Stopped the dashboard (process $($p.Id), from its pid file)."
} else {
    $listener = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
    $owner = if ($listener) { Get-CimInstance Win32_Process -Filter "ProcessId=$($listener.OwningProcess)" } else { $null }
    if ($owner -and $owner.CommandLine -match 'server\.py') {
        Stop-Process -Id $owner.ProcessId -Force
        Write-Host "Stopped the dashboard listening on port $Port (process $($owner.ProcessId)); there was no pid file for it."
    } elseif ($owner) {
        Write-Host "Port $Port is used by another program ($($owner.Name)); left it alone."
    } else {
        Write-Host 'The dashboard was not running.'
    }
}
Remove-Item $pidFile -ErrorAction SilentlyContinue
