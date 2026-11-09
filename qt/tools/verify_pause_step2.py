"""接续验证：把暂停对话框提到前台截图 → 点三角 → 抓主窗口读计时。

Windows 上 WA_TranslucentBackground 的窗口是分层窗口，PrintWindow 抓不到内容，
所以这里用"提到前台 + 按矩形抓屏"的方式。
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time

from PIL import ImageGrab

USER32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    pass

OUT = r"C:\Users\Administrator\Apps\Sudoku\dist"


def pid_of(name="Sudoku.exe"):
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout.strip()
    return int(out.split('","')[1]) if out and "No tasks" not in out else 0


def windows(pid):
    res = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        p = wt.DWORD()
        USER32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and USER32.IsWindowVisible(hwnd):
            cls = ctypes.create_unicode_buffer(128)
            USER32.GetClassNameW(hwnd, cls, 128)
            n = USER32.GetWindowTextLengthW(hwnd)
            ttl = ctypes.create_unicode_buffer(n + 1)
            USER32.GetWindowTextW(hwnd, ttl, n + 1)
            r = wt.RECT()
            USER32.GetWindowRect(hwnd, ctypes.byref(r))
            if cls.value.startswith("Qt") and "Observer" not in cls.value:
                res.append((hwnd, cls.value, ttl.value, r))
        return True

    USER32.EnumWindows(cb, 0)
    return res


def rect(hwnd):
    r = wt.RECT()
    USER32.GetWindowRect(hwnd, ctypes.byref(r))
    return r.left, r.top, r.right - r.left, r.bottom - r.top


def grab(hwnd, name, pad=0):
    USER32.SetForegroundWindow(hwnd)
    time.sleep(0.6)
    x, y, w, h = rect(hwnd)
    img = ImageGrab.grab(bbox=(x - pad, y - pad, x + w + pad, y + h + pad), all_screens=True)
    img.save(f"{OUT}\\{name}")
    print(f"  {name}: {img.size} @({x},{y})")
    return x, y, w, h


def click(x, y):
    USER32.SetCursorPos(int(x), int(y))
    time.sleep(0.1)
    USER32.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.06)
    USER32.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(0.5)


def main():
    pid = pid_of()
    if not pid:
        print("Sudoku.exe 未运行")
        return 1
    ws = windows(pid)
    main_hwnd = max(ws, key=lambda t: rect(t[0])[2] * rect(t[0])[3])[0]
    dlg = [t for t in ws if t[0] != main_hwnd]
    print(f"pid={pid} 主窗口=0x{main_hwnd:X} 其他窗口={[hex(t[0]) for t in dlg]}")

    if dlg:
        d = dlg[0][0]
        dx, dy, dw, dh = grab(d, "d2b-pause-dialog.png", pad=16)
        print(f"停止时刻（对话框居中偏下）点击三角: ({dx+dw/2:.0f},{dy+dh*0.72:.0f})")
        click(dx + dw / 2, dy + dh * 0.72)
        time.sleep(0.8)

    mx, my, mw, mh = grab(main_hwnd, "d4-resumed.png")
    print("已恢复，请看 d4 左上的计时是否仍是刚才的 00:0x")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
