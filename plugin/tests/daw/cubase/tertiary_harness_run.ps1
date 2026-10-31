# tertiary_harness_run.ps1: one Cubase release check for Tertiary, start to finish, no hands (Claude, 2026-10-29).
# Joe, 2026-10-29: "Even cubase, I think we can build a test harness project and run it through that on each run?"
#
# Lives in the Tertiary repo at plugin/tests/daw/cubase/ (moved from the Dropbox pipeline tools 2026-10-29; README.md).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File plugin\tests\daw\cubase\tertiary_harness_run.ps1
#        [-Vst3 <Tertiary.vst3 bundle>] [-MakeGolden] [-Tolerance 1e-4] [-NoInstall] [-Tools <folder>] [-Golden <folder>]
#
# Needs: the generic Cubase screen helpers cubase_windows.ps1, cubase_wait.ps1, cubase_menu.ps1 and screen.ps1, which
# stay in Joe's pipeline tools (-Tools; default $env:WLA_CUBASE_TOOLS, else Dropbox\Joe's Music\_Production\_PIPELINE\tools),
# and Audio\harness-input.wav, rebuilt by "python tertiary_harness_build.py audio" (not in git).
#
# 1. Install: copies the newest built Tertiary.vst3 (or -Vst3) to the per-user VST3 folder
#    (%LOCALAPPDATA%\Programs\Common\VST3, which Cubase 12 scans; no admin). Only with Cubase closed: if Cubase is
#    running the script stops and says so. It never closes or kills Cubase itself.
# 2. Opens "Tertiary harness.cpr" (the folder "Tertiary harness" next to this script) by launching Cubase with its path.
# 3. File > Export > Audio Mixdown with the project's saved settings (Stereo Out, locators bar 1 to 13, 48 kHz
#    32-bit float, name "Tertiary harness", project Mixdown folder), Export Audio.
#    Works since 2026-10-29 night. Before that the click "did nothing" because Cubase's audio engine had no device:
#    Stereo Out went to the TV's HDMI audio, which disappears when the TV is off. Cubase's output is now VB-Audio
#    Virtual Cable ("CABLE Input"), which is always there (README.md, "Audio output").
# 4. Quits Cubase with Ctrl+Q. The export marks the project changed, so Cubase asks to save: the script answers
#    Don't Save for the harness only (exact buttons and the harness as the only open project), otherwise it stops.
# 5. Moves the export into Runs\<date-time>\harness.wav and compares it with the golden using the repo's
#    plugin\tests\regression\compare_renders.py. -MakeGolden stores this run's render as the golden instead (only
#    after listening to it). Goldens are per machine (this Cubase on this PC), so they live outside the repo, in
#    %USERPROFILE%\.wla-test\goldens\tertiary\cubase\ (-Golden to change). Runs\ is git-ignored.
#
# Screen-control rules (HQ agent/README.md): every step that can raise a window is followed by a check of the front
# window; an unknown window means a screenshot, nothing clicked, and exit 2. Save, don't-save, overwrite and install
# prompts are never answered. Exit 0 = pass, 1 = render differs, 2 = stopped on a window, 3 = could not start.
param([string]$Vst3 = '', [switch]$MakeGolden, [double]$Tolerance = 1e-4, [switch]$NoInstall, [string]$Compare = '',
      [string]$Tools = '', [string]$Golden = '',
      [string]$Cubase = 'C:\Program Files\Steinberg\Cubase 12\Cubase12.exe', [string]$RunDir = '')
$ErrorActionPreference = 'Continue'
if (-not $Tools) { $Tools = if ($env:WLA_CUBASE_TOOLS) { $env:WLA_CUBASE_TOOLS } else { Join-Path $env:USERPROFILE "Dropbox\Joe's Music\_Production\_PIPELINE\tools" } }
$T = $Tools
$H = Join-Path $PSScriptRoot 'Tertiary harness'
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$Cpr = Join-Path $H 'Tertiary harness.cpr'
# Cubase 12 exports to the project's Mixdown folder; older settings used the project folder itself
$Exports = @((Join-Path $H 'Mixdown\Tertiary harness.wav'), (Join-Path $H 'Tertiary harness.wav'))
$Export = $Exports[0]
if (-not $Golden) { $Golden = Join-Path $env:USERPROFILE '.wla-test\goldens\tertiary\cubase' }
$UserVst3 = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
$stamp = Get-Date -Format 'yyyy-MM-dd-HHmmss'
$Run = if ($RunDir) { $RunDir } else { Join-Path $H "Runs\$stamp" }
New-Item -ItemType Directory -Force $Run | Out-Null
# [Codex 2026-10-30] The CPR stores an absolute export path; root runs own their output.
if ($RunDir) {
  $ExportFolder = Join-Path $Run 'export'
  New-Item -ItemType Directory -Force $ExportFolder | Out-Null
  $Exports = @((Join-Path $ExportFolder 'Tertiary harness.wav'))
  $Export = $Exports[0]
}
$Log = Join-Path $Run 'run.log'
# Dropbox briefly locks files it is syncing: every log write retries instead of failing the run
function Append($path, $text) { for ($k = 0; $k -lt 20; $k++) { try { [IO.File]::AppendAllText($path, $text + "`r`n"); return } catch { Start-Sleep -Milliseconds 250 } } }
function Say($m) { $line = "{0}  {1}" -f (Get-Date -Format 'HH:mm:ss'), $m; $line; Append $Log $line }
function Windows { & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\cubase_windows.ps1" }
function Front { (Windows) | Where-Object { $_ -match '^\*' } | Select-Object -First 1 }
function Shot($name) { $s = & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" shot "harness-$stamp-$name"; $p = ($s -replace '^shot ', ''); if (Test-Path $p) { Copy-Item $p (Join-Path $Run "$name.png") }; $p }
function Result($verdict, $detail) { Append (Join-Path $Run 'results.log') ("{0}  {1,-7} {2}" -f $stamp, $verdict, $detail) }
function Stop-Run($m, $code = 2) { Say "STOP: $m"; Say ("screenshot: " + (Shot 'stop')); Say 'Nothing was clicked after this. Cubase is left as it is (never killed).'; Result 'STOPPED' $m; exit $code }

Say "Tertiary harness run $stamp"
if (-not (Test-Path -LiteralPath $Cpr)) { Say "no harness project at $Cpr"; Result 'STOPPED' 'no harness project'; exit 3 }
$need = @('cubase_windows.ps1', 'cubase_wait.ps1', 'cubase_menu.ps1', 'screen.ps1' | Where-Object { -not (Test-Path (Join-Path $T $_)) })
if ($need) { Say "Cubase helpers missing from ${T}: $($need -join ', ') (pass -Tools <folder>)"; Result 'STOPPED' 'helpers missing'; exit 3 }
if (-not (Test-Path -LiteralPath (Join-Path $H 'Audio\harness-input.wav'))) { Say "no Audio\harness-input.wav (not in git): run  python `"$PSScriptRoot\tertiary_harness_build.py`" audio  first"; Result 'STOPPED' 'no input audio'; exit 3 }
$sess = (query session 2>$null) -join "`n"
if ($sess -notmatch '>?console\s+Joe\s+\d+\s+Active') { Say "Joe's session is not on the console (Remote Desktop?): screen work would fail. $sess"; Result 'STOPPED' 'session not on console'; exit 3 }

# 1. Install (Cubase must be closed)
if (Get-Process Cubase12 -ErrorAction SilentlyContinue) { Say 'Cubase is running. Close it with Ctrl+Q first: the plug-in can only be replaced with Cubase closed.'; Result 'STOPPED' 'Cubase was open'; exit 3 }
if (-not $NoInstall) {
  if (-not $Vst3) {
    $gh = Join-Path $env:USERPROFILE 'Documents\GitHub'
    $cands = @(Get-Item -Path "$gh\Tertiary*\*\plugin\Tertiary_artefacts\Release\VST3\Tertiary.vst3", "$gh\Tertiary*\*\*\plugin\Tertiary_artefacts\Release\VST3\Tertiary.vst3" -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -notmatch '\\Tertiary-(ui-tests|web)\\' -and (Test-Path (Join-Path $_.FullName 'Contents\x86_64-win\Tertiary.vst3')) } |
      Sort-Object { (Get-Item (Join-Path $_.FullName 'Contents\x86_64-win\Tertiary.vst3')).LastWriteTime } -Descending)
    if (-not $cands) { Say 'no built Tertiary.vst3 found'; Result 'STOPPED' 'no build'; exit 3 }
    $Vst3 = $cands[0].FullName
  }
  $bin = Join-Path $Vst3 'Contents\x86_64-win\Tertiary.vst3'
  $hash = (Get-FileHash $bin -Algorithm SHA256).Hash.Substring(0, 16)
  $dest = Join-Path $UserVst3 'Tertiary.vst3'
  $old = if (Test-Path (Join-Path $dest 'Contents\x86_64-win\Tertiary.vst3')) { (Get-FileHash (Join-Path $dest 'Contents\x86_64-win\Tertiary.vst3') -Algorithm SHA256).Hash.Substring(0, 16) } else { 'none' }
  Say ("build: $Vst3 (built {0}, sha256 {1})" -f (Get-Item $bin).LastWriteTime.ToString('yyyy-MM-dd HH:mm'), $hash)
  if ($old -eq $hash) { Say 'per-user VST3 already this build' }
  else {
    New-Item -ItemType Directory -Force $UserVst3 | Out-Null
    & robocopy $Vst3 $dest /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { Say "robocopy failed ($LASTEXITCODE)"; Result 'STOPPED' 'install failed'; exit 3 }
    Say "installed to $dest (was $old)"
  }
} else {
  $bin = Join-Path $UserVst3 'Tertiary.vst3\Contents\x86_64-win\Tertiary.vst3'
  $hash = (Get-FileHash $bin -Algorithm SHA256).Hash.Substring(0, 16)
  Say "no install; per-user VST3 sha256 $hash"
}
foreach ($e in $Exports) { if (Test-Path -LiteralPath $e) { Move-Item -LiteralPath $e (Join-Path $Run ('stale-' + (Split-Path $e -Leaf))); Say "moved a leftover export out of the way (no overwrite prompt): $e" } }

# 2. Open the harness
$t0 = Get-Date
# [Codex 2026-10-30] This is the interactive test host; its windows must be visible to the existing harness.
$CubaseProcess = Start-Process -FilePath $Cubase -ArgumentList ('"' + $Cpr + '"') -PassThru
# [Codex 2026-10-30] The root wrapper may clean up only this exact launched PID.
$CubaseProcess.Id | Set-Content -Encoding ASCII (Join-Path $Run 'owned-cubase.pid')
Say 'Cubase started with the harness'
$w = & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\cubase_wait.ps1" -Want '^Cubase Pro Project - Tertiary harness' -Timeout 180 -ProjectFolder $H -OwnedPid $CubaseProcess.Id
$code = $LASTEXITCODE
$w | ForEach-Object { Say "  $_" }
if ($code -ne 0) { Stop-Run "the harness did not open cleanly (cubase_wait exit $code)" }
Start-Sleep -Seconds 2
# [Codex 2026-10-30] Cubase can focus its menu frame after the project loads.
& powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" focus '^Cubase Pro Project - Tertiary harness$' | ForEach-Object { Say "  $_" }
$f = Front
if ($f -notmatch '"Cubase Pro Project - Tertiary harness"' -and $f -notmatch '"Cubase Pro"\s+SmtgMain') { Stop-Run "unexpected window in front after opening: $f" }
Say ("project open after {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)

# [Codex 2026-10-30] Prove which copy Cubase loaded, not only which copy we staged.
$processName = [IO.Path]::GetFileNameWithoutExtension($Cubase)
$loaded = @((Get-Process -Id $CubaseProcess.Id -ErrorAction Stop).Modules | Where-Object { [IO.Path]::GetFileName($_.FileName) -eq 'Tertiary.vst3' })
if ($loaded.Count -ne 1) { Stop-Run "expected one loaded Tertiary VST3 module; found $($loaded.Count)" }
$loadedHash = (Get-FileHash -LiteralPath $loaded[0].FileName -Algorithm SHA256).Hash.Substring(0, 16)
Say "Cubase loaded: $($loaded[0].FileName); sha256 $loadedHash"
if ($loadedHash -ne $hash) { Stop-Run "Cubase loaded a different build ($loadedHash, expected $hash)" }

# 3. Export Audio Mixdown with the saved settings
& powershell -NoProfile -ExecutionPolicy Bypass -File "$T\cubase_menu.ps1" run 'File>Export>Audio Mixdown...' | ForEach-Object { Say "  $_" }
$dlg = $null
# Look at all of Cubase's windows, not only the front one: another app (Chrome, VS Code) can take the front.
for ($i = 0; $i -lt 30 -and -not $dlg; $i++) {
  Start-Sleep -Milliseconds 500
  $all = Windows
  $hit = $all | Where-Object { $_ -match '"Export Audio Mixdown"\s+\S+\s+(-?\d+),(-?\d+)\s+(\d+)x(\d+)' } | Select-Object -First 1
  if ($hit) { [void]($hit -match '"Export Audio Mixdown"\s+\S+\s+(-?\d+),(-?\d+)\s+(\d+)x(\d+)'); $dlg = $Matches; break }
  $odd = $all | Where-Object { $_ -match '^\*?\s*"([^"]+)"' -and $Matches[1] -notmatch '^(Cubase Pro Project - Tertiary harness|Cubase Pro|Uninitialized 12)$' } | Select-Object -First 1
  if ($odd) { Stop-Run "unexpected window instead of Export Audio Mixdown: $odd" }
}
if (-not $dlg) { Stop-Run 'Export Audio Mixdown did not open' }
if ($RunDir) {
  & python "$T\cubase_export.py" $CubaseProcess.Id $Export | ForEach-Object { Say "  $_" }
  if ($LASTEXITCODE -ne 0) { Stop-Run 'could not set and verify this run''s export directory' }
}
$x = [int]$dlg[1] + 819; $y = [int]$dlg[2] + 852          # "Export Audio" button, bottom right of the 935x905 dialog
& powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" focus '^Export Audio Mixdown$' | Out-Null
$f = Front
if ($f -notmatch '"Export Audio Mixdown"') { Stop-Run "could not bring Export Audio Mixdown to the front (front: $f); not clicking" }
Shot 'export-dialog' | Out-Null
& powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" click $x $y | Out-Null
$tExport = ((Get-Date) - $t0).TotalSeconds
Say ("Export Audio pressed {0:N0} s after launch" -f $tExport)
if ($tExport -gt 24) { Say 'WARNING: over 24 s from launch: an unactivated (demo) Tertiary may bypass itself inside the 24.8 s render' }
$done = $false; $lastSize = -1
for ($i = 0; $i -lt 240 -and -not $done; $i++) {
  Start-Sleep -Milliseconds 500
  $f = Front
  if ($f -match '"Cubase Pro Project - Tertiary harness"' -or -not $f -or $f -match '^\*\s+"Cubase Pro"\s+SmtgMain') {
    $hitE = @($Exports | Where-Object { Test-Path -LiteralPath $_ })[0]; if ($hitE) { $Export = $hitE }
    if (Test-Path -LiteralPath $Export) { $s = (Get-Item -LiteralPath $Export).Length; if ($s -gt 0 -and $s -eq $lastSize) { $done = $true }; $lastSize = $s }
  } elseif ($f -match '"(Export Audio Mixdown|Perform Audio Export|Audio Export|Progress)"' -or $f -match '^\*\s+""\s') { }
  else { Stop-Run "unexpected window during the export: $f" }
}
if (-not $done) { Stop-Run 'the export did not finish within 2 minutes' }
Say ("exported {0:N1} MB" -f ((Get-Item -LiteralPath $Export).Length / 1MB))

# 4. Quit with Ctrl+Q (never kill)
& powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" focus '^Cubase Pro Project - Tertiary harness' | Out-Null
if ((Front) -notmatch '"Cubase Pro Project - Tertiary harness"') { Start-Sleep 1; & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" focus '^Cubase Pro Project - Tertiary harness' | Out-Null }
if ((Front) -match '"Cubase Pro Project - Tertiary harness"') { & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" key '^q' | Out-Null }
else { Say "WARNING: could not bring the project to the front for Ctrl+Q (front: $(Front))" }
$quit = $false; $answered = $false
for ($i = 0; $i -lt 60; $i++) {
  Start-Sleep -Seconds 1
  if (-not (Get-Process Cubase12 -ErrorAction SilentlyContinue)) { $quit = $true; break }
  $f = Front
  if ($f -and $f -notmatch '"Cubase Pro Project - Tertiary harness"' -and $f -notmatch 'SmtgMain') {
    # The export marks the project changed, so Ctrl+Q asks "Do you want to save the project 'Tertiary harness.cpr'?".
    # Known-safe answer, for this harness only (HQ agent/README.md, screen-control rules; Joe's brief 2026-10-29):
    # Don't Save, and only when the prompt's buttons are exactly Save / Don't Save / Cancel and the harness is the
    # only project open. Anything else: stop, nothing clicked.
    $txt = & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\cubase_windows.ps1" -Text
    $btns = @($txt | Select-Object -Skip 1 | ForEach-Object { if ($_ -match '^\s+- \[Button\] (.+)$') { $Matches[1] } else { '|' } }) -join ','
    $btns = ($btns -split ',\|')[0]
    $projects = @($txt | Where-Object { $_ -match '"Cubase Pro Project - ' })
    if (-not $answered -and $f -match '^\*\s+"Cubase Pro"\s+SteinbergWindowClass' -and $btns -eq "Save,Don't Save,Cancel" -and $projects.Count -eq 1 -and $projects[0] -match '"Cubase Pro Project - Tertiary harness"') {
      Say "save prompt for the harness on quit: Don't Save (the harness .cpr stays as it is in git)"
      & powershell -NoProfile -ExecutionPolicy Bypass -File "$T\screen.ps1" button "Don't Save" | Out-Null
      $answered = $true; continue
    }
    break
  }
}
$quitNote = ''
if ($quit) { Say 'Cubase closed' } else { $quitNote = "Cubase did not close; front window: $(Front)"; Say "WARNING: $quitNote (left open, nothing clicked)"; Say ("screenshot: " + (Shot 'quit')) }

# 5. Compare with the golden
Move-Item -LiteralPath $Export (Join-Path $Run 'harness.wav')
if (-not $Compare) { $Compare = Join-Path $Repo 'plugin\tests\regression\compare_renders.py' }
$gold = Join-Path $Golden 'harness.wav'
if ($MakeGolden) {
  New-Item -ItemType Directory -Force $Golden | Out-Null
  # an old golden goes into replaced\: compare_renders.py checks every WAV in the golden folder itself
  if (Test-Path -LiteralPath $gold) { New-Item -ItemType Directory -Force (Join-Path $Golden 'replaced') | Out-Null; Move-Item -LiteralPath $gold (Join-Path $Golden "replaced\harness.replaced-$stamp.wav") }
  Copy-Item (Join-Path $Run 'harness.wav') $gold
  Set-Content -LiteralPath (Join-Path $Golden 'golden-info.txt') -Value "Golden made $stamp from run $Run`r`nbuild sha256 $hash`r`nCubase Pro 12, mini PC, Export Audio Mixdown, Stereo Out, bar 1 to 13, 48 kHz 32-bit float"
  Say "stored as the golden: $gold"
  Result 'GOLDEN' "build $hash; export at $([int]$tExport) s $quitNote"
  exit 0
}
if (-not (Test-Path -LiteralPath $gold)) { Say 'no golden yet: listen to this run, then make it the golden with -MakeGolden'; Result 'NOGOLD' "build $hash"; exit 1 }
$out = & python $Compare $Golden $Run --tolerance $Tolerance --report (Join-Path $Run 'compare.txt') 2>&1
$cmp = $LASTEXITCODE
$out | ForEach-Object { Say "  $_" }
if ($cmp -eq 0) { Say 'PASS'; Result 'PASS' "build $hash; $(($out | Select-Object -Last 1)) $quitNote"; if ($quitNote) { exit 2 }; exit 0 }
Say 'FAIL: the render differs from the golden'; Result 'FAIL' "build $hash; $(($out | Select-Object -Last 1)) $quitNote"; exit 1
