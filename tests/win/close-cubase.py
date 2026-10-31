"""[Codex 2026-10-30] Cleanup for ONLY the Cubase PID this test started.

Try Cubase's own Ctrl+Q, dismiss only the exact harness save prompt. After 30s,
capture the failure and terminate that PID as the documented last-resort fallback.
Never attach to a Cubase instance that was open before the test.
"""
import sys
import time
import os
import threading
from pathlib import Path
import psutil
from pywinauto import Application

pid, out = int(sys.argv[1]), Path(sys.argv[2])
try:
    proc = psutil.Process(pid)
except psutil.NoSuchProcess:
    sys.exit(0)
if not proc.name().lower().startswith('cubase'):
    raise RuntimeError('PID is no longer Cubase; refusing to touch it')
finished = threading.Event()
def watchdog():
    # A hung host can block SendMessage inside a UI library. Bound that too.
    if not finished.wait(45) and proc.is_running():
        print('FAIL Cubase quit helper timed out; final fallback for owned PID', pid, flush=True)
        try:
            from PIL import ImageGrab
            ImageGrab.grab(all_screens=True).save(out / 'cubase-quit-watchdog.png')
        except Exception:
            pass
        proc.kill()
        try:
            proc.wait(timeout=10)
        finally:
            os._exit(1)
threading.Thread(target=watchdog, daemon=True).start()
try:
    app = Application(backend='win32').connect(process=pid, timeout=5)
    win = app.top_window()
    win.set_focus()
    win.type_keys('{ESC}^q', pause=0.2)
    deadline = time.monotonic() + 30
    while proc.is_running() and time.monotonic() < deadline:
        windows = app.windows()
        projects = [w for w in windows if w.window_text().startswith('Cubase Pro Project - ')]
        for w in windows:
            buttons = w.children(class_name='Button')
            names = [b.window_text().replace('&', '') for b in buttons]
            if names == ['Save', "Don't Save", 'Cancel'] and len(projects) == 1 and 'harness' in projects[0].window_text().lower():
                buttons[1].click()
        time.sleep(0.5)
except Exception as error:
    print('Quit attempt:', repr(error))
if proc.is_running():
    try:
        from PIL import ImageGrab
        ImageGrab.grab(all_screens=True).save(out / 'cubase-quit-timeout.png')
    finally:
        print('FAIL Cubase did not quit: last-resort termination of owned PID', pid)
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except psutil.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=10)
    sys.exit(1)
finished.set()
print('PASS Cubase closed through its own Quit')
