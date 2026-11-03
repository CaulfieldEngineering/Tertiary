# smoke_standalone.py
# Written by Claude (2026-10-29). A short smoke test of a JUCE plugin's standalone app on Windows, through Windows UI
# Automation (JUCE's accessibility), the way a screen reader or a person with a keyboard would use it. Generic: every
# plugin-specific name comes in on the command line (Tertiary's are in plugin/tests/run_all.ps1).
#
#   python smoke_standalone.py <standalone.exe>
#       --toggle "Low Sync"                  a toggle to flip (and flip back)
#       --appears "Low Rate"                 a control that must be showing once the toggle is flipped (optional)
#       --slider "Low Depth"                 a slider to move; its value label must follow
#       --menu-at 727,25.75 --layout 750x478 where the editor's menu icon is, in the editor's own layout units
#       --menu-item "About Tertiary...|Activate Tertiary..."   the menu item that opens the About window (any of these)
#       --dialog-button "Activate|Deactivate this computer"    a button that shows once the About window is open
#       --close-button X                     the About window's close button
#       --protect <file>                     a settings file the app writes: copied first and put back after (repeatable)
#       --timeout 120                        the whole test, in seconds
#
# It launches the app, finds the controls by name, toggles, moves the slider, opens the About window from the menu,
# closes it, and quits. The app is never left running: a watchdog kills it at the timeout, and it is killed on any
# failure. The real mouse is never moved: controls are used through UI Automation, and the one click (the menu icon,
# which is drawn rather than a control) is posted to the window as a message. Prints PASS/FAIL lines; exit code 0 = pass.

import argparse, ctypes, os, shutil, subprocess, sys, tempfile, threading, time

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("--toggle", required=True)
    ap.add_argument("--appears")
    ap.add_argument("--slider", required=True)
    ap.add_argument("--menu-at", required=True)
    ap.add_argument("--layout", required=True)
    ap.add_argument("--menu-item", required=True)
    ap.add_argument("--dialog-button", required=True)
    ap.add_argument("--close-button", default="X")
    ap.add_argument("--protect", action="append", default=[])
    ap.add_argument("--timeout", type=float, default=120)
    ap.add_argument("--screenshots", help="[Codex 2026-10-30] Save a failure image before closing the app")
    a = ap.parse_args()

    from pywinauto import Application, Desktop

    failures = []
    def check(ok, what, detail=""):
        print(("PASS " if ok else "FAIL ") + "SMOKE " + what + (("   (" + detail + ")") if detail else ""), flush=True)
        if not ok:
            failures.append(what)
        return ok

    # the settings the app writes (window size, audio device...) are put back afterwards
    backup = tempfile.mkdtemp(prefix="smoke-settings-")
    saved = []
    for f in a.protect:
        f = os.path.expandvars(f)
        saved.append((f, os.path.exists(f)))
        if os.path.exists(f):
            shutil.copy2(f, os.path.join(backup, str(len(saved))))

    proc = subprocess.Popen([os.path.abspath(a.exe)])
    killed = threading.Event()
    def watchdog():
        if not killed.wait(a.timeout):
            print("FAIL SMOKE timeout: the app was still running after %d s and was killed" % a.timeout, flush=True)
            proc.kill()
    threading.Thread(target=watchdog, daemon=True).start()

    try:
        app = Application(backend="uia").connect(process=proc.pid, timeout=30)
        win = app.top_window()
        win.wait("exists ready", timeout=30)
        check(True, "the app opened", win.window_text())
        time.sleep(1.5)   # the editor builds its accessibility tree once asked

        def find(name, control_type=None, timeout=10):
            end = time.time() + timeout
            while time.time() < end:
                for d in win.descendants():
                    ei = d.element_info
                    if ei.name == name and (control_type is None or ei.control_type == control_type):
                        return d
                time.sleep(0.2)
            return None

        # 1. a toggle, by name
        t = find(a.toggle, "CheckBox")
        if check(t is not None, "found the toggle \"%s\"" % a.toggle):
            before = t.get_toggle_state()
            t.toggle()
            time.sleep(0.4)
            after = t.get_toggle_state()
            check(after != before, "toggled \"%s\"" % a.toggle, "%s -> %s" % (before, after))
            if a.appears:
                shown = find(a.appears, timeout=3)
                check(shown is not None and shown.is_visible(), "\"%s\" shows after the toggle" % a.appears)
            t.toggle()
            time.sleep(0.4)
            check(t.get_toggle_state() == before, "toggled back")

        # 2. a slider, by name; its value label follows
        s = find(a.slider, "Slider")
        if check(s is not None, "found the slider \"%s\"" % a.slider):
            # its value label: the text field on the same row, just to the right of the slider
            sr = s.rectangle()
            def label_text():
                row = [d for d in win.descendants(control_type="Edit")
                       if abs(d.rectangle().mid_point().y - sr.mid_point().y) <= 3 and 0 <= d.rectangle().left - sr.right <= 20]
                return row[0].element_info.name if row else None
            text_before = label_text()
            lo, hi = s.min_value(), s.max_value()
            target = lo + 0.8 * (hi - lo)
            s.set_value(target)
            time.sleep(0.4)
            text_after = label_text()
            check(text_before is not None, "found its value label", str(text_before))
            check(abs(s.value() - target) <= 0.02 * (hi - lo), "moved \"%s\"" % a.slider, "%.2f, wanted %.2f" % (s.value(), target))
            check(text_after != text_before, "its value label follows", "%s -> %s" % (text_before, text_after))

        # 3. the About window, from the menu icon (drawn, not a control: a click posted to the window at its place)
        lw, lh = [float(v) for v in a.layout.split("x")]
        mx, my = [float(v) for v in a.menu_at.split(",")]
        editor = None
        for d in win.descendants(control_type="Custom"):
            r = d.rectangle()
            if r.width() > 100 and abs(r.width() / max(1, r.height()) - lw / lh) < 0.01:
                editor = r
                break
        if check(editor is not None, "found the editor in the window"):
            scale = editor.width() / lw
            pt = ctypes.wintypes.POINT(int(editor.left + mx * scale), int(editor.top + my * scale))
            hwnd = win.handle
            ctypes.windll.user32.ScreenToClient(hwnd, ctypes.byref(pt))
            lparam = (pt.y << 16) | (pt.x & 0xFFFF)
            post = ctypes.windll.user32.PostMessageW
            post(hwnd, 0x0200, 0, lparam)        # WM_MOUSEMOVE
            post(hwnd, 0x0201, 0x0001, lparam)   # WM_LBUTTONDOWN
            post(hwnd, 0x0202, 0, lparam)        # WM_LBUTTONUP
            item = None
            names = a.menu_item.split("|")
            end = time.time() + 5
            while item is None and time.time() < end:
                # [Claude 2026-10-30] Our own windows by Win32 first, then UI Automation on those only. A UIA listing of
                # the whole desktop asks every app, and one hung app (a frozen Cubase) made it take 20 s, longer than
                # this 5 s wait: the menu was there but never looked at. The Win32 listing takes no time.
                for w32 in Desktop(backend="win32").windows(process=proc.pid):
                    top = Desktop(backend="uia").window(handle=w32.handle).wrapper_object()
                    for d in top.descendants(control_type="MenuItem"):
                        if d.element_info.name in names:
                            item = d
                            break
                    if item: break
                time.sleep(0.05)
            if check(item is not None, "the menu opened, with \"%s\"" % a.menu_item):
                item.invoke()
                time.sleep(0.6)
                buttons = a.dialog_button.split("|")
                shown = None
                for n in buttons:
                    shown = shown or find(n, "Button", timeout=2)
                if check(shown is not None, "the About window opened"):
                    x = find(a.close_button, "Button", timeout=2)
                    if check(x is not None, "found its close button"):
                        x.invoke()
                        time.sleep(0.5)
                        still = [d for d in win.descendants(control_type="Button") if d.element_info.name in buttons and d.is_visible()]
                        check(not still, "closed it")

        # 4. quit
        win.close()
        try:
            proc.wait(timeout=15)
            check(proc.returncode == 0, "quit", "exit code %s" % proc.returncode)
        except subprocess.TimeoutExpired:
            check(False, "quit: the app didn't close within 15 s (killed)")
    except Exception as e:
        check(False, "unexpected error: %r" % (e,))
    finally:
        # [Codex 2026-10-30] Capture while the failing app is still visible, before cleanup.
        if failures and a.screenshots:
            try:
                from PIL import ImageGrab
                os.makedirs(a.screenshots, exist_ok=True)
                ImageGrab.grab(all_screens=True).save(os.path.join(a.screenshots, "standalone-failure.png"))
            except Exception as error:
                print("Screenshot unavailable: %r" % error, flush=True)
        killed.set()
        if proc.poll() is None:
            proc.kill()
            proc.wait(timeout=10)
        for i, (f, existed) in enumerate(saved, 1):
            copy = os.path.join(backup, str(i))
            if existed:
                shutil.copy2(copy, f)
            elif os.path.exists(f):
                os.remove(f)
        shutil.rmtree(backup, ignore_errors=True)

    print("SMOKE TEST " + ("PASSED" if not failures else "FAILED: " + "; ".join(failures)), flush=True)
    return 0 if not failures else 1

if __name__ == "__main__":
    import ctypes.wintypes
    sys.exit(main())
