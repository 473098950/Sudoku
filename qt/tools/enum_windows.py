"""枚举 Sudoku 进程的所有顶层窗口（用于定位暂停对话框）。"""
import ctypes
import ctypes.wintypes as wt
import sys

USER32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    pass


def main(pid=None):
    if pid is None:
        # 从进程名找 pid
        import subprocess
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Sudoku.exe", "/FO", "CSV", "/NH"],
                             capture_output=True, text=True).stdout.strip()
        if not out or "No tasks" in out:
            print("Sudoku.exe 未运行")
            return 1
        pid = int(out.split('","')[1])
    print("pid =", pid)

    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        p = wt.DWORD()
        USER32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value != pid:
            return True
        cls = ctypes.create_unicode_buffer(128)
        USER32.GetClassNameW(hwnd, cls, 128)
        n = USER32.GetWindowTextLengthW(hwnd)
        ttl = ctypes.create_unicode_buffer(n + 1)
        USER32.GetWindowTextW(hwnd, ttl, n + 1)
        r = wt.RECT()
        USER32.GetWindowRect(hwnd, ctypes.byref(r))
        found.append((hwnd, cls.value, ttl.value, r.left, r.top,
                      r.right - r.left, r.bottom - r.top,
                      bool(USER32.IsWindowVisible(hwnd))))
        return True

    USER32.EnumWindows(cb, 0)
    for hwnd, cls, ttl, x, y, w, h, vis in found:
        print(f"  hwnd=0x{hwnd:X} class={cls:<28} title={ttl!r:<12} "
              f"rect=({x},{y},{w}x{h}) visible={vis}")
    return 0


if __name__ == "__main__":
    sys.exit(main(int(sys.argv[1]) if len(sys.argv) > 1 else None))
