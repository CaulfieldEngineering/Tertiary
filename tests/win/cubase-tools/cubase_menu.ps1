# cubase_menu.ps1: run any Cubase menu item by its name, with no mouse, no coordinates and no screenshot.
# Cubase's menu bar is a real Windows menu, owned by a hidden Cubase window (title "Uninitialized 12" on Cubase 12).
# This walks that menu and sends Cubase the item's command, exactly as if it had been clicked.
#
#   powershell -NoProfile -File cubase_menu.ps1 list [filter]          print every item as a path, its {shortcut} and state
#                                                                       (optionally only paths matching filter)
#   powershell -NoProfile -File cubase_menu.ps1 run "File>Import>Track Archive..."
#   powershell -NoProfile -File cubase_menu.ps1 run "Project>Project Setup..."
# Paths are the visible names joined by ">", without the & accelerators and without the shortcut text after a tab.
# Matching ignores case; the last part may be a unique prefix ("File>Import>Track Arch").
# Cubase must be idle (no modal dialog open), or the command is ignored. Check the result with cubase_windows.ps1.
param([Parameter(Position = 0)] [string]$Action = 'list', [Parameter(Position = 1)] [string]$Path = '')
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class CM {
  [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr m);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr m, uint id, StringBuilder s, int n, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr m, int p);
  [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr m, int p);
  [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr m, uint id, uint f);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowThreadProcessId(IntPtr h, out int pid);
  public static List<IntPtr> Tops() { var l = new List<IntPtr>(); EnumWindows((h, p) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
}
"@
$proc = Get-Process Cubase12 -ErrorAction SilentlyContinue
if (-not $proc) { 'Cubase is not running'; exit 1 }
$owner = $null; $bar = [IntPtr]::Zero
foreach ($h in [CM]::Tops()) {
  $p = 0; [void][CM]::GetWindowThreadProcessId($h, [ref]$p)
  if ($p -eq $proc.Id) { $m = [CM]::GetMenu($h); if ($m -ne [IntPtr]::Zero -and [CM]::GetMenuItemCount($m) -gt 5) { $owner = $h; $bar = $m; break } }
}
if (-not $owner) { 'no Cubase menu bar found'; exit 1 }

function Raw($menu, $pos) { $s = New-Object Text.StringBuilder 512; [void][CM]::GetMenuString($menu, [uint32]$pos, $s, 512, 0x400); $s.ToString() }
function Name($menu, $pos) {
  $x = [string][char]1
  ((Raw $menu $pos) -split "`t")[0].Replace('&&', $x).Replace('&', '').Replace($x, '&')
}
function Keys($menu, $pos) { $p = (Raw $menu $pos) -split "`t"; if ($p.Count -gt 1) { $p[1] } else { '' } }
function Walk($menu, $prefix) {
  # WM_INITMENUPOPUP lets Cubase fill submenus it builds on demand
  [void][CM]::SendMessage($owner, 0x0117, $menu, [IntPtr]::Zero)
  $n = [CM]::GetMenuItemCount($menu)
  for ($i = 0; $i -lt $n; $i++) {
    $state = [CM]::GetMenuState($menu, [uint32]$i, 0x400)   # for a submenu the high byte is its item count
    $sub = [CM]::GetSubMenu($menu, $i)
    $name = Name $menu $i
    if (-not $name -or $name -eq '~-~' -or ($sub -eq [IntPtr]::Zero -and ($state -band 0x800))) { continue }   # separator
    $path = if ($prefix) { "$prefix>$name" } else { $name }
    if ($sub -ne [IntPtr]::Zero) { Walk $sub $path }
    else { [pscustomobject]@{ Path = $path; Id = [CM]::GetMenuItemID($menu, $i); Keys = (Keys $menu $i); Disabled = [bool]($state -band 0x3); Checked = [bool]($state -band 0x8) } }
  }
}
$items = @(Walk $bar '')
switch ($Action) {
  'list' {
    $items | ForEach-Object {
      $flag = @(); if ($_.Disabled) { $flag += 'disabled' }; if ($_.Checked) { $flag += 'on' }
      "{0,6}  {1}{2}{3}" -f $_.Id, $_.Path, $(if ($_.Keys) { "   {" + $_.Keys + "}" } else { '' }), $(if ($flag) { "   [" + ($flag -join ',') + "]" } else { '' }) } |
      Where-Object { -not $Path -or $_ -match [regex]::Escape($Path) }
  }
  'run' {
    # Cubase adds state to some menu titles, e.g. "Workspaces (G1)" once a workspace is active: ignore " (...)" when matching
    $norm = { param($p) ($p -replace '\s*\([^)>]*\)(?=>|$)', '').ToLower() }
    $want = & $norm $Path
    $hit = @($items | Where-Object { (& $norm $_.Path) -eq $want })
    if (-not $hit) { $hit = @($items | Where-Object { (& $norm $_.Path).StartsWith($want) }) }
    if ($hit.Count -ne 1) { "no single match for '$Path' ($($hit.Count) matches)"; $hit | ForEach-Object { "  " + $_.Path }; exit 2 }
    if ($hit[0].Disabled) { "'$($hit[0].Path)' is disabled right now"; exit 3 }
    [void][CM]::PostMessage($owner, 0x0111, [IntPtr]$hit[0].Id, [IntPtr]::Zero)   # WM_COMMAND
    "ran '$($hit[0].Path)' (id $($hit[0].Id))"
  }
}
