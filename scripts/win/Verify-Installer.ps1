# [Claude 2026-10-31] B-05 Verify installer (Windows): the installer and every plug-in/app binary carry a valid
# Authenticode signature, then <installer>.sha256 is written (the publish job uploads only a file whose hash still
# matches). Moved unchanged from workflow.yml "Verify Windows Installer and Binaries Are Signed".
# -RequireSigned (GitHub, when signing ran): unsigned is FAIL. Without it (a desk build): unsigned is SKIP, listed.
param([string]$BuildDir = 'build', [string]$Config = 'Release', [switch]$RequireSigned, [switch]$DryRun)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
Start-Step 'B-05' 'Verify installer' -DryRun:$DryRun

$dir = Resolve-InRepo $BuildDir
$version = $Cfg['wPROJECT_VERSION']
$exe = @(Get-ChildItem (Join-Path $dir "*WinInstaller_$version.exe") -ErrorAction SilentlyContinue)
if ($DryRun) { Complete-Step SKIP "dry run: would check $($exe.Count) installer(s) and the binaries under $dir" }
if ($exe.Count -ne 1) { Fail-Step "Expected one Windows installer for $version, found $($exe.Count)" }
$artefacts = Join-Path $dir "plugin\$($Cfg['wPLUGIN_NAME'])_artefacts\$Config"
$files = @($exe[0].FullName) + @(Get-ChildItem $artefacts -Recurse -File -Include *.exe, *.vst3 | ForEach-Object FullName)
if ($files.Count -lt 2) { Fail-Step "No plug-in or app binaries found under $artefacts" }
$unsigned = @()
foreach ($f in $files) {
    $sig = Get-AuthenticodeSignature $f
    if ($sig.Status -ne 'Valid') {
        if ($RequireSigned) { Fail-Step "Not validly signed ($($sig.Status)): $f" }
        $unsigned += (Split-Path $f -Leaf) + " ($($sig.Status))"
        continue
    }
    Write-Host "OK  $f  ($($sig.SignerCertificate.Subject))"
}
$hash = (Get-FileHash $exe[0].FullName -Algorithm SHA256).Hash.ToLower()
[IO.File]::WriteAllText("$($exe[0].FullName).sha256", $hash)
Add-Artifact "$($exe[0].FullName).sha256"
if ($unsigned) { Complete-Step SKIP ("unsigned desk build, nothing to verify: " + ($unsigned -join ', ') + "; sha256 $hash") }
Complete-Step PASS "Windows installer verified: $($exe[0].Name)  sha256 $hash"
