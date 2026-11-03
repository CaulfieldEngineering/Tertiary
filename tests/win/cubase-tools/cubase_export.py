"""[Codex 2026-10-30] Set and verify Cubase 12's export destination in the owned test dialog.

The CPR retains an absolute export path even after choosing its new project folder.
No export is clicked until the absolute filename is read back in the native chooser.
"""
import json
from pathlib import Path
import sys
import psutil
from pywinauto import Desktop, mouse
from pywinauto.controls.win32_controls import EditWrapper

pid, destination = int(sys.argv[1]), Path(sys.argv[2]).resolve()
if not psutil.Process(pid).name().lower().startswith('cubase'):
    raise RuntimeError('The owned PID is not Cubase')
destination.parent.mkdir(parents=True, exist_ok=True)
if destination.exists():
    raise RuntimeError('Refusing to overwrite an existing export')
windows = Desktop(backend='win32').windows(process=pid)
dialog = next(w for w in windows if w.window_text() == 'Export Audio Mixdown')
rect = dialog.rectangle()
print('Export dialog:', rect, flush=True)
controls = [(w.class_name(), w.window_text(), str(w.rectangle())) for w in dialog.descendants()]
print(json.dumps(controls), flush=True)
# The known Cubase 12 layout at 100% display scale. Unknown geometry must be reviewed.
if not (915 <= rect.width() <= 940 and 890 <= rect.height() <= 915):
    raise RuntimeError('Unrecognized Cubase export geometry; no fields changed')
dialog.set_focus()
mouse.double_click(coords=(rect.left + 610, rect.top + 134))
chooser = Desktop(backend='win32').window(process=pid, title='Choose location and filename')
chooser.wait('visible', timeout=20)
chooser.child_window(control_id=1001, class_name='Edit').wait('visible enabled', timeout=20)
edits = chooser.descendants(class_name='Edit')
print('Chooser edits:', [(e.control_id(), e.window_text()) for e in edits], flush=True)
edit = next((e for e in edits if e.control_id() == 1001), None)
if edit is None:
    raise RuntimeError('Expected the native filename Edit (1001); no path typed')
edit = EditWrapper(edit.handle)
edit.set_edit_text(str(destination))
if edit.window_text() != str(destination):
    raise RuntimeError('Absolute export filename did not read back correctly')
buttons = chooser.descendants(class_name='Button')
print('Chooser buttons:', [b.window_text() for b in buttons], flush=True)
save = next((b for b in buttons if b.window_text().replace('&', '') == 'Save'), None)
if save is None:
    raise RuntimeError('Expected the native Save button; nothing confirmed')
# [Claude 2026-10-31] W-08 failed once (repeat run, 14:18): the filename read back correctly but the chooser
# ignored the Save click and stayed open for 20 s. A dropped click is retried, bounded: Save again, then Enter
# in the filename field, each followed by a 10 s wait. The filename is re-read before every retry and the
# export itself is still verified by the harness afterwards; nothing else changes.
from pywinauto.timings import TimeoutError as WaitTimeout
for attempt, confirm in enumerate((lambda: save.click(), lambda: save.click(),
                                   lambda: (edit.set_focus(), edit.type_keys('{ENTER}')))):
    if attempt and edit.window_text() != str(destination):
        raise RuntimeError('The export filename changed before retrying Save; nothing confirmed')
    confirm()
    try:
        chooser.wait_not('visible', timeout=10)
        break
    except WaitTimeout:
        others = [w.window_text() for w in Desktop(backend='win32').windows(process=pid) if w.is_visible()]
        print(f'Save attempt {attempt + 1} left the chooser open; visible windows: {others}', flush=True)
else:
    raise RuntimeError('The chooser stayed open after three Save attempts')
print('PASS export filename accepted: ' + str(destination), flush=True)
