# Small screen driver for Cubase (and any app). Several steps in one call, run left to right:
#   click x y          left click at screen pixel x,y
#   rclick x y         right click
#   dclick x y         double click
#   sclick x y         Shift+click (extend a selection)
#   wheel x y n        mouse wheel at x,y: n notches, negative scrolls down (never click a scrollbar's zoom handle)
#   drag x1 y1 x2 y2   press at x1,y1, move to x2,y2, release (resize a window by its edge, move a part)
#   key "<sendkeys>"   key chord in SendKeys syntax: "^n" Ctrl+N, "+s" Shift+S, "%f" Alt+F, "{ENTER}", "{ESC}"
#   type "<text>"      literal text (special characters escaped for you)
#   wait s             pause s seconds
#   shot name          full-screen screenshot to %TEMP%\cubase-screens\name.png
#   focus "<regex>"    bring the first visible window whose title matches to the front (restores it if minimized)
#   button "<text>"    press a named button in the front window, wherever it is (Windows dialogs only:
#                      file/folder pickers, message boxes; Cubase's own dialogs draw their buttons themselves)
#   close "<regex>"    close every visible window whose title matches (sends a normal close, like its X button;
#                      fine for Cubase's Markers / Tempo / MixConsole windows, never use it on the project window)
#   minimize "<regex>" minimize every visible window whose title matches (e.g. the Codex app that steals the front)
# Example: powershell -NoProfile -File screen.ps1 focus "^Cubase Pro Project" key "+s" wait 1 shot setup
param([Parameter(ValueFromRemainingArguments = $true)] $steps)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class UI {
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 public static void Btn(uint down, uint up, int x, int y) { SetCursorPos(x, y); System.Threading.Thread.Sleep(120); mouse_event(down,0,0,0,UIntPtr.Zero); mouse_event(up,0,0,0,UIntPtr.Zero); }
 public static void Click(int x, int y) { Btn(2, 4, x, y); }
 public static List<KeyValuePair<IntPtr,string>> Windows() { var l = new List<KeyValuePair<IntPtr,string>>(); EnumWindows((h, p) => { if (IsWindowVisible(h)) { var s = new StringBuilder(512); GetWindowText(h, s, 512); if (s.Length > 0) l.Add(new KeyValuePair<IntPtr,string>(h, s.ToString())); } return true; }, IntPtr.Zero); return l; }
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 public static string Press(string text) { IntPtr fg = GetForegroundWindow(); IntPtr hit = IntPtr.Zero; string want = text.Replace("&", "");
   EnumChildWindows(fg, (h, p) => { var c = new StringBuilder(64); GetClassName(h, c, 64); var t = new StringBuilder(256); GetWindowText(h, t, 256);
     if (c.ToString() == "Button" && t.ToString().Replace("&", "") == want) { hit = h; return false; } return true; }, IntPtr.Zero);
   if (hit == IntPtr.Zero) return "button: no '" + text + "' in the front window"; SendMessage(hit, 0x00F5, IntPtr.Zero, IntPtr.Zero); return "pressed '" + text + "'"; }
 public static bool Focus(IntPtr h) { if (IsIconic(h)) ShowWindow(h, 9); keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero); return SetForegroundWindow(h); }
}
"@
$dir = Join-Path $env:TEMP 'cubase-screens'; New-Item -ItemType Directory -Force $dir | Out-Null
for ($i = 0; $i -lt $steps.Count; $i++) {
  switch ($steps[$i]) {
    'click'  { [UI]::Click([int]$steps[$i+1], [int]$steps[$i+2]); $i += 2; Start-Sleep -Milliseconds 900 }
    'rclick' { [UI]::Btn(8, 16, [int]$steps[$i+1], [int]$steps[$i+2]); $i += 2; Start-Sleep -Milliseconds 900 }
    'drag'   { $x1, $y1, $x2, $y2 = [int]$steps[$i+1], [int]$steps[$i+2], [int]$steps[$i+3], [int]$steps[$i+4]
               [UI]::SetCursorPos($x1, $y1); Start-Sleep -Milliseconds 150; [UI]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
               for ($s = 1; $s -le 12; $s++) { [UI]::SetCursorPos($x1 + ($x2-$x1)*$s/12, $y1 + ($y2-$y1)*$s/12); Start-Sleep -Milliseconds 30 }
               [UI]::mouse_event(4,0,0,0,[UIntPtr]::Zero); $i += 4; Start-Sleep -Milliseconds 700 }
    'wheel'  { [UI]::SetCursorPos([int]$steps[$i+1], [int]$steps[$i+2]); Start-Sleep -Milliseconds 100; [UI]::mouse_event(0x800, 0, 0, [uint32]([int]$steps[$i+3] * 120 -band 0xFFFFFFFF), [UIntPtr]::Zero); $i += 3; Start-Sleep -Milliseconds 500 }
    'sclick' { [UI]::keybd_event(0x10, 0, 0, [UIntPtr]::Zero); [UI]::Click([int]$steps[$i+1], [int]$steps[$i+2]); [UI]::keybd_event(0x10, 0, 2, [UIntPtr]::Zero); $i += 2; Start-Sleep -Milliseconds 900 }
    'dclick' { [UI]::Click([int]$steps[$i+1], [int]$steps[$i+2]); Start-Sleep -Milliseconds 80; [UI]::Click([int]$steps[$i+1], [int]$steps[$i+2]); $i += 2; Start-Sleep -Milliseconds 900 }
    'key'    { [System.Windows.Forms.SendKeys]::SendWait($steps[$i+1]); $i += 1; Start-Sleep -Milliseconds 700 }
    'type'   { [System.Windows.Forms.SendKeys]::SendWait(($steps[$i+1] -replace '([+^%~(){}\[\]])', '{$1}')); $i += 1; Start-Sleep -Milliseconds 500 }
    'wait'   { Start-Sleep -Seconds ([double]$steps[$i+1]); $i += 1 }
    'shot'   { $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height; $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen(0,0,0,0,$bmp.Size); $p = Join-Path $dir ($steps[$i+1] + '.png'); $bmp.Save($p); "shot $p"; $i += 1 }
    'focus'  { $w = [UI]::Windows() | Where-Object { $_.Value -match $steps[$i+1] } | Select-Object -First 1
               if ($w) { "focus '$($w.Value)': $([UI]::Focus($w.Key))" } else { "focus: no window matches '$($steps[$i+1])'" }; $i += 1; Start-Sleep -Milliseconds 500 }
    'button' { [UI]::Press($steps[$i+1]); $i += 1; Start-Sleep -Milliseconds 900 }
    'close'  { [UI]::Windows() | Where-Object { $_.Value -match $steps[$i+1] } | ForEach-Object { [void][UI]::SendMessage($_.Key, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero); "closed '$($_.Value)'" }; $i += 1; Start-Sleep -Milliseconds 500 }
    'minimize' { [UI]::Windows() | Where-Object { $_.Value -match $steps[$i+1] } | ForEach-Object { [void][UI]::ShowWindow($_.Key, 6); "minimized '$($_.Value)'" }; $i += 1; Start-Sleep -Milliseconds 400 }
  }
}
