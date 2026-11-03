"""[Codex 2026-10-30] Named startup dialogs and native Quit for an owned REAPER.

The product driver must defer rendering until ``ready`` is true. In particular,
rendering while the evaluation notice is counting down can strand its modal loop.
Only the PID returned by that driver's Popen is inspected or sent commands.
Requires pywinauto and its Windows dependencies (pywin32 and Pillow).
"""
import ctypes
import json
from pathlib import Path
import re
import subprocess
import sys
import time


def capture_window(pid, handle, destination):
    """Render the owned window itself, avoiding unrelated foreground overlays."""
    import win32gui
    import win32process
    import win32ui
    from PIL import Image
    if win32process.GetWindowThreadProcessId(handle)[1] != pid:
        raise RuntimeError('Window no longer belongs to the test REAPER process')
    left, top, right, bottom = win32gui.GetWindowRect(handle)
    width, height = right - left, bottom - top
    window_dc = win32gui.GetWindowDC(handle)
    source = win32ui.CreateDCFromHandle(window_dc)
    target = source.CreateCompatibleDC()
    bitmap = win32ui.CreateBitmap()
    try:
        bitmap.CreateCompatibleBitmap(source, width, height)
        target.SelectObject(bitmap)
        if not ctypes.windll.user32.PrintWindow(handle, target.GetSafeHdc(), 2):
            raise RuntimeError('PrintWindow failed')
        Image.frombuffer('RGB', (width, height), bitmap.GetBitmapBits(True),
                         'raw', 'BGRX', 0, 1).save(destination)
    finally:
        win32gui.DeleteObject(bitmap.GetHandle())
        target.DeleteDC()
        source.DeleteDC()
        win32gui.ReleaseDC(handle, window_dc)


class ReaperWindows:
    def __init__(self, pid, work):
        from pywinauto import Desktop
        self.desktop = Desktop(backend='win32')
        self.pid = pid
        self.work = Path(work)
        self.started = time.monotonic()
        self.state = None
        self.screenshots = set()
        self.last_dialog = self.started
        self.main_seen = None
        self.last_quit = 0
        self.ready = False

    def event(self, event, **detail):
        row = dict(seconds=round(time.monotonic() - self.started, 3), event=event, **detail)
        with (self.work / 'windows.log').open('a', encoding='utf-8') as log:
            log.write(json.dumps(row) + '\n')
        if event != 'windows':
            print('REAPER UI ' + json.dumps(row), flush=True)

    def screenshot(self, window, name):
        if name in self.screenshots:
            return
        try:
            # PrintWindow may wait on a hung window, so run it under its own bound.
            subprocess.run([sys.executable, __file__, '--capture', str(self.pid),
                            str(window.handle), str(self.work / (name + '.png'))],
                           check=True, capture_output=True, timeout=3)
            self.screenshots.add(name)
        except Exception as error:
            self.event('screenshot-error', name=name, error=str(error))

    @staticmethod
    def label(button):
        return button.window_text().replace('&', '').strip().casefold()

    def poll(self, finished=False):
        windows = self.desktop.windows(process=self.pid, visible_only=True)
        state = [dict(title=w.window_text(), cls=w.class_name(),
                      buttons=[dict(name=b.window_text(), enabled=b.is_enabled())
                               for b in w.children(class_name='Button')]) for w in windows]
        if state != self.state:
            self.event('windows', windows=state)
            self.state = state
        dialogs = [w for w in windows if w.class_name() == '#32770']
        notices = [w for w in dialogs if w.window_text() == 'REAPER New Version Notification']
        for window in dialogs:
            title = window.window_text()
            buttons = window.children(class_name='Button')
            if title == 'REAPER New Version Notification':
                self.screenshot(window, 'reaper-version-notification')
                button = next((b for b in buttons if self.label(b) == 'close' and b.is_enabled()), None)
            elif re.fullmatch(r'About REAPER v.+', title):
                # An update notice can steal focus after About first appears.
                # Let it close first; keep About active while honouring its timer.
                if not notices and ctypes.windll.user32.GetForegroundWindow() != window.handle:
                    window.set_focus()
                    self.event('focus-evaluation', title=title)
                if not notices:
                    self.screenshot(window, 'reaper-evaluation-countdown')
                button = next((b for b in buttons if self.label(b) == 'still evaluating' and b.is_enabled()), None)
                if button:
                    self.screenshot(window, 'reaper-still-evaluating')
            else:
                button = None
                self.screenshot(window, 'reaper-dialog-' + re.sub(r'[^a-z0-9]+', '-', title.casefold()).strip('-'))
            if button:
                self.event('click-named-button', title=title, button=button.window_text())
                # Asynchronous BM_CLICK keeps the driver's watchdog bounded.
                ctypes.windll.user32.PostMessageW(button.handle, 0x00F5, 0, 0)
        now = time.monotonic()
        if dialogs:
            self.last_dialog = now
        main = next((w for w in windows if w.class_name() == 'REAPERwnd'), None)
        if main and self.main_seen is None:
            self.main_seen = now
        # The startup grace permits REAPER to create its asynchronous notices.
        # Count from the main window, since plugin scans can precede it by 20 s.
        # Unlike the previous fixed sleep, a present dialog always blocks render.
        self.ready = bool(main) and not dialogs and now - self.main_seen >= 8 and now - self.last_dialog >= 1
        if finished and self.ready and now - self.last_quit >= 2:
            self.event('native-quit', command='File: Quit (40004)')
            ctypes.windll.user32.PostMessageW(main.handle, 0x0111, 40004, 0)
            self.last_quit = now
        return self.ready

    def capture_timeout(self):
        for index, window in enumerate(self.desktop.windows(process=self.pid, visible_only=True)):
            if window.class_name() in ('#32770', 'REAPERwnd'):
                self.screenshot(window, 'reaper-timeout-' + str(index))


if __name__ == '__main__':
    if len(sys.argv) != 5 or sys.argv[1] != '--capture':
        raise SystemExit('Use through the REAPER test driver')
    capture_window(int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
