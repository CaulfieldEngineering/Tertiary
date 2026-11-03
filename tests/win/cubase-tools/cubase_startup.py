"""[Codex 2026-10-30] Known Cubase 12 Safe Mode prompt after an owned test timeout.

Keep current preferences, then OK. Never disable or delete preferences. Unknown
dialog geometry is refused. PID comes from the harness's Start-Process result.
"""
import sys
from pathlib import Path
import psutil
from pywinauto import Desktop, mouse

pid = int(sys.argv[1])
if not psutil.Process(pid).name().lower().startswith('cubase'):
    raise RuntimeError('The owned PID is not Cubase')
for win in Desktop(backend='win32').windows(process=pid):
    buttons = {b.window_text(): b for b in win.children(class_name='Button')}
    if (len(sys.argv) > 2 and Path(sys.argv[2]).is_dir() and win.window_text() == 'Cubase Pro'
            and set(buttons) == {'New (1)', 'Old (2)', 'Cancel'}):
        buttons['New (1)'].click()
        print('Selected the relocated harness directory: ' + sys.argv[2], flush=True)
        continue
    if win.window_text() != 'Safe Mode':
        continue
    rect = win.rectangle()
    if (rect.width(), rect.height()) != (1126, 553):
        raise RuntimeError('Unrecognized Cubase Safe Mode geometry; not clicking')
    if set(buttons) != {'OK', 'Cancel'}:
        raise RuntimeError('Unrecognized Safe Mode buttons; not clicking')
    win.set_focus()
    # Cubase draws these radio controls itself. This is the verified Cubase 12,
    # 100% display layout: Use current program preferences, not Disable/Delete.
    mouse.click(coords=(rect.left + 44, rect.top + 325))
    buttons['OK'].click()
    print('Kept current preferences in the owned Cubase Safe Mode prompt', flush=True)
