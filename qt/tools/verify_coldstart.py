"""冷启动回归验证
场景：settings.ini 里残留 paused=1（上次是暂停着退出的），
期望：新进程启动后直接是"运行态"——不弹暂停层、计时继续走、胶囊是时钟图标。
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time
from pathlib import Path

import wincap

USER32 = ctypes.windll.user32
OUT = Path(r"C:\Users\Administrator\Apps\Sudoku\dist")
EXE = OUT / "Sudoku.exe"
INI = OUT / "settings.ini"

# 胶囊区域（抓取图坐标系，540×685 窗口实测）
PILL = (10, 40, 130, 96)
ACCENT = (0x5b, 0x67, 0xe8)


def kill():
    subprocess.run(["taskkill", "/F", "/IM", "Sudoku.exe"], capture_output=True)
    time.sleep(0.8)


def pid_of():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Sudoku.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True, errors="ignore").stdout.strip()
    return int(out.split('","')[1]) if out and "No tasks" not in out else 0


def main_window(pid, timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
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
            _, _, w, h = wincap.window_rect(hwnd)
            if w * h > best[1]:
                best = (hwnd, w * h)
            return True

        USER32.EnumWindows(cb, 0)
        if best[0]:
            return best[0]
        time.sleep(0.3)
    return 0


def brightness(img):
    px = list(img.convert("L").getdata())
    return sum(px) / len(px)


def accent_pixels(img):
    n = 0
    for r, g, b in img.getdata():
        if (abs(r - ACCENT[0]) < 45 and abs(g - ACCENT[1]) < 45 and abs(b - ACCENT[2]) < 45):
            n += 1
    return n


def diff_count(a, b):
    pa, pb = list(a.getdata()), list(b.getdata())
    return sum(1 for x, y in zip(pa, pb)
               if abs(x[0] - y[0]) + abs(x[1] - y[1]) + abs(x[2] - y[2]) > 24)


def read_ini():
    d = {}
    for line in INI.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            d[k.strip()] = v.strip()
    return d


def set_ini(key, val):
    lines = INI.read_text(encoding="utf-8").splitlines()
    for i, line in enumerate(lines):
        if line.startswith(key + "="):
            lines[i] = f"{key}={val}"
            break
    else:
        lines.append(f"{key}={val}")
    INI.write_text("\r\n".join(lines) + "\r\n", encoding="utf-8")


def main():
    kill()
    old = read_ini()
    print(f"[准备] 造一个残留暂停态：seconds={old.get('seconds')} -> paused=1")
    set_ini("paused", "1")
    sec_before = int(old.get("seconds", 0))

    subprocess.Popen([str(EXE)], cwd=str(OUT))
    pid = 0
    for _ in range(25):
        pid = pid_of()
        if pid:
            break
        time.sleep(0.2)
    h = main_window(pid)
    if not h:
        print("未找到主窗口 / FAIL")
        return 1
    print(f"[启动] pid={pid} 主窗口=0x{h:X}")

    time.sleep(1.3)
    w0 = wincap.capture(h)
    w0.save(OUT / "f1-coldstart-t0.png")
    p0 = w0.crop(PILL)
    p0.resize((p0.width * 3, p0.height * 3)).save(OUT / "f1-coldstart-pill.png")
    b0, a0 = brightness(w0), accent_pixels(p0)
    print(f"[t0] 整窗亮度={b0:.1f}  胶囊 accent 像素={a0}")

    time.sleep(6.0)
    w1 = wincap.capture(h)
    w1.save(OUT / "f2-coldstart-t6s.png")
    p1 = w1.crop(PILL)
    p1.resize((p1.width * 3, p1.height * 3)).save(OUT / "f2-coldstart-pill-6s.png")
    b1, a1 = brightness(w1), accent_pixels(p1)
    d = diff_count(p0, p1)
    print(f"[t+6s] 整窗亮度={b1:.1f}  胶囊 accent 像素={a1}  胶囊像素差={d}")

    USER32.PostMessageW(h, 0x0010, 0, 0)     # WM_CLOSE
    time.sleep(1.5)
    kill()
    after = read_ini()
    print(f"[关闭] seconds={after.get('seconds')} paused={after.get('paused')}")

    sec_after = int(after.get("seconds", 0))
    ok_no_overlay = b0 > 28 and a0 < 80        # 没被 paused 层压暗；胶囊非 accent(play)
    ok_ticking = d > 25                        # 6 秒后时间数字变了
    ok_clocked = sec_after - sec_before >= 4   # 秒数在涨
    ok_ini = after.get("paused") in ("0", None)

    print("\n---- 判定 ----")
    print(f"  不继承暂停态(无暂停层/时钟图标) : {'PASS' if ok_no_overlay else 'FAIL'}")
    print(f"  胶囊 6 秒内有变化(计时在走)     : {'PASS' if ok_ticking else 'FAIL'}")
    print(f"  seconds {sec_before} -> {sec_after:<6}          : {'PASS' if ok_clocked else 'FAIL'}")
    print(f"  运行中 paused 已回写 0          : {'PASS' if ok_ini else 'FAIL'}")
    print("RESULT:", "COLD_START_OK" if all([ok_no_overlay, ok_ticking, ok_clocked, ok_ini])
          else "COLD_START_FAIL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
