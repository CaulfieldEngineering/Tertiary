# site-screenshots.ps1
# Written by Claude (2026-10-30). One command to re-make the wonderlandaudio.com screenshots of Tertiary's window
# after a UI change. From the repo root:
#
#   powershell -ExecutionPolicy Bypass -File plugin\review\site-screenshots.ps1 [-Build build-review] [-Out site-screenshots]
#
# It builds ReviewRender (configuring build-review with -DWLA_REVIEW_RENDER=ON the first time), then runs
# site_screenshots.py, which holds the settings (preset, sample, 2x scale, crop). Needs Python with Pillow.
# Then upload the four .webp files and swap their addresses into the two HTML widgets on the home page; how is in the site notes,
# "Wonderland Audio Site Drafts 2026-10-28\CHANGES-2026-10-30.md", section "Screenshots of the new UI".

param(
    [string]$Build = "build-review",
    [string]$Out = "site-screenshots",
    [string]$Sample = "plugin\sample_audio\Organ.wav"   # fills the spectrum more than the guitar does
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root

if (-not (Test-Path (Join-Path $Build "CMakeCache.txt"))) {
    cmake -S . -B $Build -DWLA_REVIEW_RENDER=ON
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}
cmake --build $Build --config Release --target ReviewRender
if ($LASTEXITCODE -ne 0) { throw "ReviewRender build failed" }

$exe = Join-Path $Build "plugin\ReviewRender_artefacts\Release\ReviewRender.exe"
python (Join-Path $PSScriptRoot "site_screenshots.py") $exe $Sample $Out
if ($LASTEXITCODE -ne 0) { throw "render failed" }
Write-Host "Written to $Out (tertiary-main-window-1x/-2x.webp, tertiary-scope-and-crossover-1x/-2x.webp)"
