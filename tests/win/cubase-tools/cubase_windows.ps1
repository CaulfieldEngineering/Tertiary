# cubase_windows.ps1: list Cubase's visible top-level windows (title, class, rect) so a script can read
# Cubase's state without a screenshot: which project is open, whether it is modified, which dialog is up.
# Usage: powershell -NoProfile -File cubase_windows.ps1 [-All] [-Text]
#   -All   include hidden windows    -Text  also dump the child-window text of each dialog (buttons, labels)
param([switch]$All, [switch]$Text)
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class CW {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowThreadProcessId(IntPtr h, out int pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  public struct RECT { public int L, T, R, B; }
  public static List<IntPtr> Tops() { var l = new List<IntPtr>(); EnumWindows((h, p) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
  public static List<IntPtr> Kids(IntPtr w) { var l = new List<IntPtr>(); EnumChildWindows(w, (h, p) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
  public static string T(IntPtr h) { var s = new StringBuilder(512); GetWindowText(h, s, 512); return s.ToString(); }
  public static string C(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
}
"@
$ids = @(Get-Process | Where-Object { $_.ProcessName -match 'Cubase|vstscanner' } | ForEach-Object { $_.Id })
if (-not $ids) { "(Cubase not running)"; exit 1 }
$fg = [CW]::GetForegroundWindow()
foreach ($h in [CW]::Tops()) {
  $procId = 0; [void][CW]::GetWindowThreadProcessId($h, [ref]$procId)
  if ($ids -notcontains $procId) { continue }
  if (-not $All -and -not [CW]::IsWindowVisible($h)) { continue }
  $r = New-Object CW+RECT; [void][CW]::GetWindowRect($h, [ref]$r)
  $title = [CW]::T($h); $mark = if ($h -eq $fg) { '*' } else { ' ' }
  "{0} {1,-48} {2,-28} {3},{4} {5}x{6}" -f $mark, ('"' + $title + '"'), [CW]::C($h), $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T)
  if ($Text) { foreach ($k in [CW]::Kids($h)) { $t = [CW]::T($k); if ($t) { "      - [{0}] {1}" -f [CW]::C($k), $t } } }
}
