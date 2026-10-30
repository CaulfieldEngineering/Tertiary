# installer_test_windows.ps1
# Written by Claude (2026-10-27). Tests the SIGNED Windows installer that the staging pipeline built, the way a
# customer gets it. Used by .github/workflows/installer-test.yml (windows-latest; needs administrator rights).
#
#   powershell -File installer_test_windows.ps1 <installer .exe> <HostTest.exe> <pluginval.exe>
#
# Every step runs even if an earlier one failed. A summary table goes to results\installer-summary.md.
# Exit code 1 if any step failed.

param([string]$Installer, [string]$HostTest, [string]$Pluginval)

$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force results | Out-Null
$Summary = "results\installer-summary.md"
"| Step | Result | Detail |`n|---|---|---|" | Out-File -Encoding utf8 $Summary
$script:Failed = $false

$Vst3 = "C:\Program Files\Common Files\VST3\Tertiary.vst3"
$AppDir = "C:\Program Files\Wonderland Audio\Tertiary"
$App = "$AppDir\Tertiary.exe"

# Step "<name>" { script }: the script returns $true/$false plus a detail line
function Step([string]$Name, [scriptblock]$Body) {
    try { $ok, $detail = & $Body } catch { $ok = $false; $detail = $_.Exception.Message }
    $result = if ($ok) { "PASS" } else { "**FAIL**"; $script:Failed = $true }
    Write-Host ("{0,-5} {1,-40} {2}" -f ($(if ($ok) { "PASS" } else { "FAIL" })), $Name, $detail)
    "| $Name | $result | $(([string]$detail).Replace('|', '/')) |" | Out-File -Append -Encoding utf8 $Summary
}

# Runs a program with a time limit, so a hung step is named instead of hanging the whole job
function Run([string]$Exe, [string[]]$Arguments, [string]$Log, [int]$TimeoutSeconds = 600) {
    Write-Host "running $Log ..."
    $p = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -PassThru `
         -RedirectStandardOutput "results\$Log.log" -RedirectStandardError "results\$Log.err.log"
    $null = $p.Handle   # without this, PowerShell loses the exit code of a process started without -Wait
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        return $false, "timed out after $TimeoutSeconds s"
    }
    $last = Get-Content "results\$Log.log" -Tail 1 -ErrorAction SilentlyContinue
    return ($p.ExitCode -eq 0), "exit $($p.ExitCode): $last"
}

Write-Host "=== Windows $([Environment]::OSVersion.Version), installer: $(Split-Path $Installer -Leaf)"

# In the self-test (UNSIGNED=1: installers built unsigned on a claude/installer-test branch) signatures can't
# pass, so those steps are reported as SKIP
function SignedStep([string]$Name, [scriptblock]$Body) {
    if ($env:UNSIGNED -eq "1") {
        Write-Host ("SKIP  {0,-40} unsigned self-test" -f $Name)
        "| $Name | SKIP | unsigned self-test: only the staging pipeline signs |" | Out-File -Append -Encoding utf8 $Summary
    } else { Step $Name $Body }
}

# 1. The installer as downloaded: signed (Azure Trusted Signing)
SignedStep "installer signature" {
    $s = Get-AuthenticodeSignature $Installer
    ($s.Status -eq "Valid"), "$($s.Status): $($s.SignerCertificate.Subject)"
}

# 1b. [Claude 2026-10-30] R-17: a Tertiary.vst3 bundle FOLDER already where the installer puts the single-file VST3
#     (Joe's laptop, from a manual copy: the install stopped with "MoveFile failed; code 183"). Made here, so the
#     install below must replace it.
Step "R-17 setup: a Tertiary.vst3 bundle folder already there" {
    New-Item -ItemType Directory -Force (Join-Path $Vst3 "Contents\x86_64-win") | Out-Null
    Set-Content -Path (Join-Path $Vst3 "Contents\x86_64-win\Tertiary.vst3") -Value "an old bundle copied by hand"
    (Test-Path $Vst3 -PathType Container), "folder $Vst3"
}

# 2. Silent install, as a customer would (all components, default folders)
Step "silent install" { Run $Installer @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$PWD\results\install.log`"") "install-run" 300 }

# 3. Files where DAWs look
Step "VST3 in Common Files\VST3" { (Test-Path $Vst3), $Vst3 }
Step "R-17: the VST3 is the single file; the old bundle folder is gone" {
    (Test-Path $Vst3 -PathType Leaf), $(if (Test-Path $Vst3 -PathType Container) { "still a folder" } elseif (Test-Path $Vst3) { "a file, $((Get-Item $Vst3).Length) bytes" } else { "missing" })
}
Step "standalone in Program Files" { (Test-Path $App), $App }

# 3b. [Claude 2026-10-30] Installing again over that install (an upgrade over a previous file install) still works
Step "install again over the previous install (upgrade)" {
    $before = (Get-Item $Vst3).LastWriteTimeUtc
    Start-Sleep -Seconds 2
    $ok, $detail = Run $Installer @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$PWD\results\install-again.log`"") "install-again-run" 300
    if (-not $ok) { return $false, $detail }
    $isFile = Test-Path $Vst3 -PathType Leaf
    ($isFile -and (Test-Path $App)), "$detail; VST3 a file: $isFile, written $(if ((Get-Item $Vst3).LastWriteTimeUtc -ne $before) { 'again' } else { 'with the same time stamp (same build)' })"
}

# 4. The installed binaries are signed too (the pipeline signs the installer; this shows whether the
#    plugin and app inside are signed as well)
foreach ($f in @($Vst3, $App)) {
    SignedStep "signature $(Split-Path $f -Leaf)" {
        $s = Get-AuthenticodeSignature $f
        ($s.Status -eq "Valid"), "$($s.Status)"
    }
}

# 5. pluginval and the host test on the installed VST3 (unlicensed: the demo gaps must be there)
Step "pluginval installed VST3" { Run $Pluginval @("--strictness-level", "10", "--validate-in-process", "--validate", "`"$Vst3`"") "pluginval-installed" }
Step "HostTest installed VST3" { Run $HostTest @("--vst3", "`"$Vst3`"", "--expect-demo") "hosttest-installed" }

# 6. The standalone starts, runs a few seconds, and closes cleanly when its window is closed
Step "standalone starts and quits" {
    $p = Start-Process -FilePath $App -PassThru
    $null = $p.Handle
    Start-Sleep -Seconds 6
    if ($p.HasExited) { return $false, "exited by itself, code $($p.ExitCode)" }
    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(10000)) { $p.Kill(); return $false, "still running 10 s after closing its window" }
    ($p.ExitCode -eq 0), "started, ran 6 s, closed with exit code $($p.ExitCode)"
}

# 7. Uninstall with the Inno Setup uninstaller, and confirm nothing is left
Step "uninstall and confirm clean" {
    $uninstaller = Get-ChildItem $AppDir -Filter "unins*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $uninstaller) { return $false, "no uninstaller in $AppDir" }
    $p = Start-Process $uninstaller.FullName -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -PassThru
    $null = $p.Handle
    if (-not $p.WaitForExit(300000)) { return $false, "uninstaller timed out after 300 s" }
    Start-Sleep -Seconds 5   # the uninstaller finishes in a copy of itself
    $left = @($Vst3, $App) | Where-Object { Test-Path $_ }
    $key = Get-ChildItem "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" |
           Where-Object { (Get-ItemProperty $_.PSPath).DisplayName -like "*Tertiary*" }
    if ($left -or $key) { return $false, "left behind: $($left -join ', ') $(if ($key) { 'uninstall registry entry' })" }
    $true, "files and uninstall entry removed"
}

Get-Content $Summary
if ($script:Failed) { exit 1 } else { exit 0 }
