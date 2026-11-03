# [Claude 2026-10-31] B-02 / B-04 Sign (Windows): Azure Trusted Signing of the plug-in and app (-Target plugin) or
# the installer (-Target installer).
#
# Same account, profile, digest and timestamp server as the release workflow. On GitHub the signing itself stays
# in the workflow's pinned azure/trusted-signing-action step (its secrets, identity and conditions unchanged); this
# script is the same call for a desk with the six AZURE_* variables set, through the module that action installs
# (TrustedSigning 0.5.0, current user). Without them, or with -DryRun, it lists what it would sign and reports SKIP.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\win\Sign-Windows.ps1 -Target plugin -DryRun
param(
    [ValidateSet('plugin', 'installer')][string]$Target = 'plugin',
    [string]$BuildDir = 'build',
    [string]$Config = 'Release',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\common.ps1')
$id = if ($Target -eq 'plugin') { 'B-02' } else { 'B-04' }
Start-Step $id ("Sign " + $(if ($Target -eq 'plugin') { 'plug-in and app' } else { 'installer' })) -DryRun:$DryRun

$dir = Resolve-InRepo $BuildDir
$name = $Cfg['wPLUGIN_NAME']
if ($Target -eq 'plugin') {
    $folder = Join-Path $dir "plugin\${name}_artefacts\$Config"; $filter = 'exe,vst3'; $recurse = $true
    $files = @(Get-ChildItem $folder -Recurse -File -Include *.exe, *.vst3 -ErrorAction SilentlyContinue | ForEach-Object FullName)
} else {
    $folder = $dir; $filter = 'exe'; $recurse = $false
    $files = @(Get-ChildItem (Join-Path $dir '*.exe') -File -ErrorAction SilentlyContinue | ForEach-Object FullName)
}
if (-not $files) { Fail-Step "nothing to sign in $folder (build it first)" }

$vars = 'AZURE_TENANT_ID', 'AZURE_CLIENT_ID', 'AZURE_CLIENT_SECRET', 'AZURE_ENDPOINT', 'AZURE_CODE_SIGNING_NAME', 'AZURE_CERT_PROFILE_NAME'
$missing = @($vars | Where-Object { -not [Environment]::GetEnvironmentVariable($_) })
if ($DryRun -or $missing) {
    if ($missing -and (On-Tag)) { Fail-Step 'Azure signing secrets are missing on a release tag' }
    $why = if ($DryRun) { 'dry run' } else { 'no signing credentials here (' + ($missing -join ', ') + ')' }
    Complete-Step SKIP ("$why; would sign $($files.Count) file(s): " + (($files | ForEach-Object { Split-Path $_ -Leaf }) -join ', '))
}

if (-not (Get-Module -ListAvailable TrustedSigning | Where-Object Version -eq '0.5.0')) {
    Install-Module -Name TrustedSigning -RequiredVersion 0.5.0 -Force -Scope CurrentUser -Repository PSGallery
}
Import-Module TrustedSigning -RequiredVersion 0.5.0
$p = @{
    Endpoint = $env:AZURE_ENDPOINT; CodeSigningAccountName = $env:AZURE_CODE_SIGNING_NAME
    CertificateProfileName = $env:AZURE_CERT_PROFILE_NAME; FilesFolder = $folder; FilesFolderFilter = $filter
    FileDigest = 'SHA256'; TimestampRfc3161 = 'http://timestamp.acs.microsoft.com'; TimestampDigest = 'SHA256'
}
if ($recurse) { $p['FilesFolderRecurse'] = $true }
Invoke-TrustedSigning @p
$bad = @($files | Where-Object { (Get-AuthenticodeSignature $_).Status -ne 'Valid' })
if ($bad) { Fail-Step ('not validly signed after signing: ' + ($bad -join ', ')) }
Complete-Step PASS "signed $($files.Count) file(s) in $folder"
