"""桌面端 v2 复验：暂停模型与安卓对齐 + 三角居中。

1. 冷启动 → 运行态
2. 点胶囊 → 弹出暂停卡片；4 秒内计时冻结；量三角与圆心的偏差
3. 点三角 → 卡片关闭、计时继续
4. 最小化 6 秒后还原 → 自动继续（不弹卡片、胶囊回到时钟态、计时在走）
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
    o = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Sudoku.exe", "/FO", "CSV", "/NH"],
                       capture_output=True, text=True, errors="ignore").stdout.strip()
    return int(o.split('","')[1]) if o and "No tasks" not in o else 0


def windows_of(pid):
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
            if w >= 4 and h >= 4:
                res.append({"hwnd": hwnd, "x": x, "y": y, "w": w, "h": h,
                            "main": cls.value.endswith("QWindowIcon")})
        return True

    USER32.EnumWindows(cb, 0)
    return res


def main_win(pid):
    ws = [w for w in windows_of(pid) if w["main"]]
    ws.sort(key=lambda w: w["w"] * w["h"], reverse=True)
    return ws[0] if ws else None


def dialog_of(pid, main):
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
    time.sleep(0.6)
    USER32.ShowWindow(h, 9)
    time.sleep(1.0)
    return USER32.GetForegroundWindow() == h


def diff_count(a, b):
    return sum(1 for x, y in zip(a.getdata(), b.getdata())
               if abs(x[0] - y[0]) + abs(x[1] - y[1]) + abs(x[2] - y[2]) > 24)


def measure_triangle(img):
    W, H = img.size
    px = img.load()
    minx, miny, maxx, maxy = 10 ** 6, 10 ** 6, -1, -1
    for y in range(H // 2, H):
        for x in range(W):
            r, g, b = px[x, y]
            if abs(r - ACCENT[0]) < 45 and abs(g - ACCENT[1]) < 45 and abs(b - ACCENT[2]) < 45:
                minx, maxx = min(minx, x), max(maxx, x)
                miny, maxy = min(miny, y), max(maxy, y)
    if maxx < 0:
        return None
    cx, cy = (minx + maxx) / 2, (miny + maxy) / 2
    tx, ty = [], []
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            r, g, b = px[x, y]
            if r > 190 and g > 190 and b > 190:
                tx.append(x)
                ty.append(y)
    if not tx:
        return None
    return {"circle": (cx, cy, maxx - minx + 1),
            "tri_bbox": ((min(tx) + max(tx)) / 2, (min(ty) + max(ty)) / 2),
            "tri_wh": (max(tx) - min(tx) + 1, max(ty) - min(ty) + 1)}


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
    print(f"主窗口 0x{m['hwnd']:X} {m['w']}x{m['h']}")
    wincap.capture(m["hwnd"]).save(OUT / "p1-v2-coldstart.png")

    fg = raise_(m["hwnd"])
    m = main_win(pid)
    k = min(m["w"] / DESIGN_W, m["h"] / DESIGN_H)
    PILL = (int(44 * k), int(44 * k), int(110 * k), int(94 * k))
    print(f"① 还原后前台={fg}，{m['w']}x{m['h']} k={k:.2f}")
    # 还原后应保持运行态（或刚自动恢复）：3 秒内数字要动
    a = wincap.capture(m["hwnd"]).crop(PILL)
    time.sleep(3.0)
    b = wincap.capture(m["hwnd"]).crop(PILL)
    alive_after_restore = diff_count(a, b) > 20
    print(f"   还原后 3 秒胶囊像素差={diff_count(a, b)} → {'计时在走' if alive_after_restore else '仍冻结'}")

    # ② 点胶囊 → 卡片
    click(m["x"] + 38 * k, m["y"] + 55 * k)
    dlg = dialog_of(pid, m)
    tri_click = (0, 0)
    metrics = None
    if dlg:
        d = wincap.capture(dlg["hwnd"])
        d.save(OUT / "p2-v2-pause-dialog.png")
        metrics = measure_triangle(d)
        if metrics:
            cx = metrics["circle"][0]
            cy = metrics["circle"][1]
            tri_click = (dlg["x"] + cx, dlg["y"] + cy)
            tx, ty = metrics["tri_bbox"]
            print(f"② 卡片 {d.size}：圆 d={metrics['circle'][2]} 圆心=({cx:.1f},{cy:.1f}) "
                  f"三角 {metrics['tri_wh'][0]}x{metrics['tri_wh'][1]} "
                  f"偏差 dx={tx - cx:+.1f} dy={ty - cy:+.1f}")
    print(f"   卡片={'有' if dlg else '无'}")

    # ③ 冻结 4 秒
    t0 = wincap.capture(m["hwnd"]).crop(PILL)
    time.sleep(4.0)
    t1 = wincap.capture(m["hwnd"]).crop(PILL)
    frozen = diff_count(t0, t1)
    print(f"③ 卡片开着 4 秒，胶囊像素差={frozen}")

    # ④ 点三角恢复
    run_diff = 0
    closed = False
    if dlg:
        click(*tri_click)
        closed = dialog_of(pid, m) is None
        a = wincap.capture(m["hwnd"]).crop(PILL)
        time.sleep(3.0)
        b = wincap.capture(m["hwnd"])
        b.save(OUT / "p3-v2-resumed.png")
        run_diff = diff_count(a, b.crop(PILL))
        print(f"④ 点三角：卡片{'已关闭' if closed else '仍在'}，3 秒胶囊像素差={run_diff}")

    # ⑤ 最小化 6 秒 → 还原：自动继续
    print("⑤ 最小化 6 秒后还原")
    USER32.ShowWindow(m["hwnd"], 6)
    time.sleep(6)
    USER32.ShowWindow(m["hwnd"], 9)
    time.sleep(1.0)
    m = main_win(pid)
    s0 = wincap.capture(m["hwnd"])
    s0.save(OUT / "p4-v2-after-minimize.png")
    time.sleep(3.0)
    s1 = wincap.capture(m["hwnd"])
    auto_run = diff_count(s0.crop(PILL), s1.crop(PILL))
    no_dialog = dialog_of(pid, m) is None
    print(f"   还原后 3 秒胶囊像素差={auto_run}，是否弹卡片={not no_dialog}")

    kill()
    off = None
    if metrics:
        off = (abs(metrics["tri_bbox"][0] - metrics["circle"][0]),
               abs(metrics["tri_bbox"][1] - metrics["circle"][1]))
    ok = {
        "点胶囊 弹出卡片": bool(dlg),
        "卡片开着 计时冻结": frozen == 0,
        "点三角 关闭卡片": closed,
        "点三角 计时继续": run_diff > 20,
        "最小化还原 自动继续": auto_run > 20,
        "最小化还原 不弹卡片": no_dialog,
        "三角与圆心对齐(≤1px)": bool(off and off[0] <= 1.0 and off[1] <= 1.0),
    }
    print("\n---- 判定 ----")
    for n_, v in ok.items():
        print(f"  {n_:<22}: {'PASS' if v else 'FAIL'}")
    print("RESULT:", "PAUSE_V2_OK" if all(ok.values()) else "PAUSE_V2_FAIL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
