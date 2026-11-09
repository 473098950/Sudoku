"""桌面端暂停功能自检：启动 exe → 点左上角计时器 → 截图暂停界面 → 点三角恢复。

坐标推导与 relayout() 一致（设计尺寸 432×548，k = 实际/设计）：
    计时器胶囊左上角 = (16k, 26k + 14k)，高 30k
    圆点取胶囊内部一个安全位置 (38k, 55k)
"""
import ctypes
import os
import subprocess
import sys
import time

from PIL import ImageGrab

USER32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    pass

EXE = r"C:\Users\Administrator\Apps\Sudoku\dist\Sudoku.exe"
OUT = r"C:\Users\Administrator\Apps\Sudoku\dist"
TITLE = "数独"


def find_window(timeout=15.0):
    end = time.time() + timeout
    while time.time() < end:
        h = USER32.FindWindowW(None, TITLE)
        if h:
            return h
        time.sleep(0.25)
    return 0


def rect(hwnd):
    class RECT(ctypes.Structure):
        _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                    ("right", ctypes.c_long), ("bottom", ctypes.c_long)]
    r = RECT()
    USER32.GetWindowRect(hwnd, ctypes.byref(r))
    return r.left, r.top, r.right, r.bottom


def click(x, y):
    USER32.SetCursorPos(int(x), int(y))
    time.sleep(0.08)
    USER32.mouse_event(0x0002, 0, 0, 0, 0)   # LEFTDOWN
    time.sleep(0.05)
    USER32.mouse_event(0x0004, 0, 0, 0, 0)   # LEFTUP
    time.sleep(0.35)


def shot(hwnd, name):
    l, t, r, b = rect(hwnd)
    img = ImageGrab.grab(bbox=(l, t, r, b), all_screens=True)
    p = os.path.join(OUT, name)
    img.save(p)
    print(f"  截图 {name}: {img.size}")
    return p


def main():
    hwnd = USER32.FindWindowW(None, TITLE)
    if not hwnd:
        print("启动 exe ...")
        subprocess.Popen([EXE], cwd=os.path.dirname(EXE))
        hwnd = find_window()
    if not hwnd:
        print("FAIL 找不到窗口")
        return 1

    USER32.ShowWindow(hwnd, 9)          # SW_RESTORE
    USER32.SetForegroundWindow(hwnd)
    time.sleep(0.8)

    l, t, r, b = rect(hwnd)
    w, h = r - l, b - t
    k = max(0.2, min(1.0, min(w / 432.0, h / 548.0)))
    print(f"窗口 {w}x{h} @({l},{t})  k={k:.3f}")

    shot(hwnd, "d1-running.png")

    px, py = l + 38 * k, t + 55 * k
    print(f"点击计时器 ({px:.0f},{py:.0f})")
    click(px, py)
    shot(hwnd, "d2-pause.png")

    time.sleep(4)
    shot(hwnd, "d3-pause4s.png")

    print("点击三角（对话框按窗口居中推定）")
    # 对话框在父窗口居中，宽 230，三角按钮在面板中部偏下
    cx, cy = l + w / 2, t + h / 2
    click(cx, cy + 52)
    shot(hwnd, "d4-resumed.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
