# Watchdog for Cubase prompts. Joe, 2026-10-25: "I just don't want to come back tomorrow and you got stuck on an
# unexpected prompt window." Waits until the front window's title matches -Want, answering the prompts we know are
# safe, and STOPS on anything else (screenshot + exit 2) instead of guessing or hanging.
#   powershell -File cubase_wait.ps1 -Want "^Cubase Pro Project - Octavio" [-Timeout 180]
# Exit 0: reached. Exit 2: unknown prompt (see the printed screenshot path). Exit 3: timed out.
# Known prompts and their safe answers (positions on the mini PC, 1920x1080):
#   "Missing Ports"                      -> OK           (the laptop's audio inputs are absent here)
#   "Cubase Pro" 257x126  activate?      -> Activate
#   Project moved, exact New/Old/Cancel -> New (1), only with -ProjectFolder from the test wrapper
# Never auto-answered: save / don't save / remove tracks / overwrite. Those stop the wait.
param([Parameter(Mandatory)][string]$Want, [int]$Timeout = 180, [string]$ProjectFolder = '', [int]$OwnedPid = 0)
$here = $PSScriptRoot
$known = @(
  @{ title = '^Missing Ports$';          size = $null;      x = 1143; y = 697; what = 'Missing Ports: OK' },
  @{ title = '^Cubase Pro$';             size = '257x126';  x = 916;  y = 557; what = 'Activate project: Activate' }
)
$deadline = (Get-Date).AddSeconds($Timeout)
$unknown = ''
while ((Get-Date) -lt $deadline) {
  # [Codex 2026-10-30] Recover only the known startup prompt of the PID this harness launched.
  if ($OwnedPid -gt 0) {
    & python "$here\cubase_startup.py" $OwnedPid $ProjectFolder
    if ($LASTEXITCODE -ne 0) { exit 2 }
  }
  $all = & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\cubase_windows.ps1"
  # [Codex 2026-10-30] A cloned harness has a new project directory. Choose New by its
  # exact button, regardless of dialog width; Old would export into a different worktree.
  # Only the wrapper's explicitly supplied existing harness folder enables this answer.
  if ($ProjectFolder -and (Test-Path -LiteralPath $ProjectFolder)) {
    $txt = (& powershell -NoProfile -ExecutionPolicy Bypass -File "$here\cubase_windows.ps1" -Text) -join "`n"
    if ($txt -match '\[Button\] New \(1\)' -and $txt -match '\[Button\] Old \(2\)' -and $txt -match '\[Button\] Cancel') {
      & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\screen.ps1" focus '^Cubase Pro$' | Out-Null
      $frontNow = (& powershell -NoProfile -ExecutionPolicy Bypass -File "$here\cubase_windows.ps1") | Where-Object { $_ -match '^\*' } | Select-Object -First 1
      if ($frontNow -match '"Cubase Pro"\s+SteinbergWindowClass') {
        "answered: relocated test harness, New (1): $ProjectFolder"
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\screen.ps1" button 'New (1)' | Out-Null
        Start-Sleep -Seconds 1
        continue
      }
    }
  }
  $front = $all | Where-Object { $_ -match '^\*' } | Select-Object -First 1
  # The app frame itself ("Cubase Pro", 1920x31) or nothing in front: success if the wanted window exists at all.
  if (-not $front -or $front -match '^\*\s+"Cubase Pro"\s+SmtgMain') {
    $got = $all | Where-Object { $_ -match '^\s*\*?\s+"([^"]*)"' -and $Matches[1] -match $Want } | Select-Object -First 1
    if ($got) { "reached: $got"; exit 0 }
    Start-Sleep 2; continue
  }
  if (-not $front) { Start-Sleep 2; continue }
  if ($front -notmatch '^\*\s+"([^"]*)"\s+\S+\s+[-\d]+,[-\d]+\s+(\d+x\d+)') { Start-Sleep 2; continue }
  $title, $size = $Matches[1], $Matches[2]
  if ($title -match $Want) { "reached: $title"; exit 0 }
  # A project's MixConsole can open in front of its project window: the project is there, so that counts too.
  $proj = $all | Where-Object { $_ -match '^\s*\*?\s+"([^"]*)"' -and $Matches[1] -match $Want } | Select-Object -First 1
  if ($proj -and $title -match '^MixConsole - ') { "reached (behind its MixConsole): $proj"; exit 0 }
  if ($title -match '^Loading' -or $title -match '^Reading Project File' -or $title -eq 'Uninitialized 12' -or $title -eq 'Checking Licenses...') { Start-Sleep 2; continue }
  $hit = $known | Where-Object { $title -match $_.title -and (-not $_.size -or $_.size -eq $size) } | Select-Object -First 1
  if ($hit) {
    "answered: $($hit.what)"
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\screen.ps1" click $hit.x $hit.y wait 2 | Out-Null
    continue
  }
  if ($title -match '^(Cubase Pro Project|MixConsole) - ' -or $title -match '^(VST Instruments|Edit History)') { Start-Sleep 2; continue }   # ordinary windows, not prompts
  # [Codex 2026-10-30] Startup windows can disappear between enumeration and capture.
  # Stop only on a stable unknown prompt; never click it or extend the overall deadline.
  $key = "$title/$size"
  if ($unknown -ne $key) { $unknown = $key; Start-Sleep 2; continue }
  $shot = & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\screen.ps1" shot ("unknown-prompt-" + (Get-Date -Format HHmmss))
  "UNKNOWN PROMPT '$title' $size - stopped, nothing clicked. $shot"
  & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\cubase_windows.ps1" -Text
  exit 2
}
"timed out after $Timeout s waiting for '$Want'"
exit 3
