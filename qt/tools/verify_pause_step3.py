"""验证桌面端"离开窗口自动暂停"：最小化 6 秒再还原，计时应当没走。"""
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


def main_window(pid):
    best = (0, 0)

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        nonlocal best
        p = wt.DWORD()
        USER32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value != pid:
            return True
        cls = ctypes.create_unicode_buffer(128)
        USER32.GetClassNameW(hwnd, cls, 128)
        if not cls.value.endswith("QWindowIcon"):
            return True
        r = wt.RECT()
        USER32.GetWindowRect(hwnd, ctypes.byref(r))
        area = (r.right - r.left) * (r.bottom - r.top)
        if area > best[1]:
            best = (hwnd, area)
        return True

    USER32.EnumWindows(cb, 0)
    return best[0]


def grab_pill(hwnd, name):
    r = wt.RECT()
    USER32.GetWindowRect(hwnd, ctypes.byref(r))
    w = r.right - r.left
    # 左上角胶囊区域（逻辑 16..90 × 40..72  →  物理 ×1.25，留余量）
    box = (r.left, r.top, r.left + int(w * 0.45), r.top + int(w * 0.17))
    img = ImageGrab.grab(bbox=box, all_screens=True)
    img = img.resize((img.width * 2, img.height * 2))
    img.save(f"{OUT}\\{name}")
    print(f"  {name}: {img.size}")
    return img


def main():
    pid = pid_of()
    if not pid:
        print("Sudoku.exe 未运行")
        return 1
    h = main_window(pid)
    print(f"主窗口 0x{h:X}")

    USER32.ShowWindow(h, 9)          # SW_RESTORE
    USER32.SetForegroundWindow(h)
    time.sleep(0.8)
    grab_pill(h, "e1-pill-running.png")

    print("最小化 6 秒 ...")
    USER32.ShowWindow(h, 6)          # SW_MINIMIZE
    time.sleep(6)
    USER32.ShowWindow(h, 9)          # SW_RESTORE
    USER32.SetForegroundWindow(h)
    time.sleep(1.2)
    grab_pill(h, "e2-pill-after-min.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
