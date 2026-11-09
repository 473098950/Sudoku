"""桌面端暂停功能最终复验（真实前台 + 真实鼠标点击 + PrintWindow 抓取）。

流程：
  1. 冷启动 → 运行态（时钟图标、计时在走）
  2. 点左上角胶囊 → 弹出暂停卡片（"已暂停" + 大号时间 + 唯一三角）
  3. 卡片开着 4 秒 → 主窗口计时冻结
  4. 点卡片里的三角 → 卡片关闭、计时继续
  5. 最小化 6 秒再还原 → 静默暂停（胶囊变 ▶，不弹窗）
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time
from pathlib import Path

import wincap

USER32 = ctypes.windll.user32
USER32.GetForegroundWindow.restype = wt.HWND

OUT = Path(r"C:\Users\Administrator\Apps\Sudoku\dist")
EXE = OUT / "Sudoku.exe"
ACCENT = (0x5b, 0x67, 0xe8)
DESIGN_W, DESIGN_H = 432.0, 548.0


def kill():
    subprocess.run(["taskkill", "/F", "/IM", "Sudoku.exe"], capture_output=True)
    time.sleep(0.8)


def pid_of():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Sudoku.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True, errors="ignore").stdout.strip()
    return int(out.split('","')[1]) if out and "No tasks" not in out else 0


def windows_of(pid):
    """所有可见 Qt 顶层窗口（主窗 = QWindowIcon；暂停卡片 = QWindow）。"""
    res = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        p = wt.DWORD()
        USER32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value != pid or not USER32.IsWindowVisible(hwnd):
            return True
        cls = ctypes.create_unicode_buffer(128)
        USER32.GetClassNameW(hwnd, cls, 128)
        if cls.value.startswith("Qt6") and "QWindow" in cls.value:
            x, y, w, h = wincap.window_rect(hwnd)
            if w < 4 or h < 4:
                return True
            res.append({"hwnd": hwnd, "x": x, "y": y, "w": w, "h": h,
                        "main": cls.value.endswith("QWindowIcon")})
        return True

    USER32.EnumWindows(cb, 0)
    return res


def main_win(pid):
    ws = [w for w in windows_of(pid) if w["main"]]
    ws.sort(key=lambda w: w["w"] * w["h"], reverse=True)
    return ws[0] if ws else None


def dialog_win(pid, main):
    for w in windows_of(pid):
        if w["hwnd"] != main["hwnd"]:
            return w
    return None


def click(x, y):
    USER32.SetCursorPos(int(x), int(y))
    time.sleep(0.12)
    USER32.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.06)
    USER32.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(0.6)


def raise_(h):
    USER32.ShowWindow(h, 6)
    time.sleep(0.5)
    USER32.ShowWindow(h, 9)
    time.sleep(0.9)
    return USER32.GetForegroundWindow() == h


def diff_count(a, b):
    pa, pb = list(a.getdata()), list(b.getdata())
    return sum(1 for x, y in zip(pa, pb)
               if abs(x[0] - y[0]) + abs(x[1] - y[1]) + abs(x[2] - y[2]) > 24)


def accent_centroid(img):
    W, H = img.size
    px = img.load()
    sx = sy = n = 0
    for y in range(H // 2, H):                 # 按钮在卡片下半部
        for x in range(W):
            r, g, b = px[x, y]
            if (abs(r - ACCENT[0]) < 45 and abs(g - ACCENT[1]) < 45
                    and abs(b - ACCENT[2]) < 45):
                sx += x
                sy += y
                n += 1
    return (sx / n, sy / n, n) if n else (0.0, 0.0, 0)


def main():
    kill()
    subprocess.Popen([str(EXE)], cwd=str(OUT))
    pid = 0
    for _ in range(25):
        pid = pid_of()
        if pid:
            break
        time.sleep(0.2)
    time.sleep(1.6)

    m = main_win(pid)
    print(f"主窗口 0x{m['hwnd']:X} @({m['x']},{m['y']}) {m['w']}x{m['h']}")

    # ① 冷启动运行态
    w0 = wincap.capture(m["hwnd"])
    w0.save(OUT / "n1-cold-start.png")
    k = min(m["w"] / DESIGN_W, m["h"] / DESIGN_H)
    PILL = (int(44 * k), int(44 * k), int(110 * k), int(94 * k))
    print("① 冷启动抓图完成")

    # 抬到前台（也顺带触发一次"离开窗口"）
    ok_fg = raise_(m["hwnd"])
    m = main_win(pid)
    k = min(m["w"] / DESIGN_W, m["h"] / DESIGN_H)
    PILL = (int(44 * k), int(44 * k), int(110 * k), int(94 * k))
    print(f"   前台={ok_fg}  窗口 {m['w']}x{m['h']} k={k:.2f}")

    # 还原后可能是静默暂停态，先点胶囊 → 卡片
    print("② 点左上角胶囊")
    click(m["x"] + 38 * k, m["y"] + 55 * k)
    dlg = dialog_win(pid, m)
    print(f"   卡片 = {'有 0x%X' % dlg['hwnd'] if dlg else '无'}")
    tri = (0, 0)
    if dlg:
        d = wincap.capture(dlg["hwnd"])
        d.save(OUT / "n2-pause-dialog.png")
        cx, cy, n = accent_centroid(d)
        tri = (dlg["x"] + cx, dlg["y"] + cy)
        with open(OUT / "n2-dialog-metrics.txt", "w", encoding="utf-8") as f:
            f.write(f"dialog={d.size} accent_centroid=({cx:.2f},{cy:.2f}) n={n}\n")
        print(f"   卡片 {d.size} @({dlg['x']},{dlg['y']}) 圆心≈({cx:.0f},{cy:.0f})")

    # ③ 卡片开着 4 秒，主窗口计时冻结
    t0 = wincap.capture(m["hwnd"]).crop(PILL)
    time.sleep(4.0)
    t1 = wincap.capture(m["hwnd"]).crop(PILL)
    frozen = diff_count(t0, t1)
    print(f"③ 卡片开着 4 秒，胶囊像素差={frozen}")

    # ④ 点三角恢复
    resumed_txt = "未执行"
    run_diff = 0
    if dlg:
        print("④ 点卡片里的三角")
        click(*tri)
        left = dialog_win(pid, m)
        a = wincap.capture(m["hwnd"]).crop(PILL)
        time.sleep(3.0)
        b = wincap.capture(m["hwnd"])
        b.save(OUT / "n3-resumed.png")
        run_diff = diff_count(a, b.crop(PILL))
        resumed_txt = "已关闭" if not left else "仍在"
        print(f"   卡片{resumed_txt}；恢复后 3 秒胶囊像素差={run_diff}")

    # ⑤ 最小化 6 秒 → 静默暂停
    print("⑤ 最小化 6 秒再还原")
    USER32.ShowWindow(m["hwnd"], 6)
    time.sleep(6)
    USER32.ShowWindow(m["hwnd"], 9)
    time.sleep(1.0)
    m = main_win(pid)
    s0 = wincap.capture(m["hwnd"])
    s0.save(OUT / "n4-after-minimize.png")
    time.sleep(3.0)
    s1 = wincap.capture(m["hwnd"])
    min_frozen = diff_count(s0.crop(PILL), s1.crop(PILL))
    min_dialog = dialog_win(pid, m) is not None
    print(f"   最小化后 胶囊像素差={min_frozen}  是否弹窗={min_dialog}")

    kill()
    ok = {
        "点胶囊 弹出暂停卡片": bool(dlg),
        "卡片打开期间 计时冻结": frozen == 0,
        "点三角 关闭卡片": resumed_txt == "已关闭",
        "恢复后 计时继续": run_diff > 20,
        "最小化 静默暂停(冻结)": min_frozen == 0,
        "最小化 不弹窗": not min_dialog,
    }
    print("\n---- 判定 ----")
    for n_, v in ok.items():
        print(f"  {n_:<20}: {'PASS' if v else 'FAIL'}")
    print("RESULT:", "PAUSE_OK" if all(ok.values()) else "PAUSE_FAIL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
