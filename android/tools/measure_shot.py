"""从真机截图里量出实际几何，与 SudokuView.computeLayout() 的公式对照。

不靠肉眼：直接按调色板颜色（classic-dark）在像素上找按键方框与数字字形的边界。
"""
import sys
import numpy as np
from PIL import Image

BGPX = (0x16, 0x1A, 0x29)      # p.bg
SURF = (0x21, 0x26, 0x3A)      # p.surface


def load(path):
    im = Image.open(path).convert("RGB")
    a = np.asarray(im).astype(np.int16)
    return a, im.size


def near(a, rgb, tol=10):
    return (np.abs(a[:, :, 0] - rgb[0]) <= tol) & \
           (np.abs(a[:, :, 1] - rgb[1]) <= tol) & \
           (np.abs(a[:, :, 2] - rgb[2]) <= tol)


def main(path):
    a, (W, H) = load(path)
    print(f"截图尺寸: {W}x{H}")

    surf = near(a, SURF, 12)
    bg = near(a, BGPX, 10)

    # ---- 1. 找键盘带：整行里 surface 像素数最多的一段（连续 40 行以上）
    row_surf = surf.sum(axis=1)
    # 从下往上，找第一个 surface 占比 > 15% 宽度的行
    band_bottom = None
    for y in range(H - 1, H // 2, -1):
        if row_surf[y] > W * 0.15:
            band_bottom = y
            break
    if band_bottom is None:
        print("未找到键盘带")
        return
    y = band_bottom
    while y > 0 and row_surf[y] > W * 0.15:
        y -= 1
    band_top = y + 1
    print(f"键盘带 y: {band_top}..{band_bottom}  高度={band_bottom - band_top + 1}px")

    # ---- 2. 该带内找 9 个按键方框的左右边界（取带中线那一行）
    ym = (band_top + band_bottom) // 2
    row = surf[ym]
    runs, s = [], None
    for x in range(W):
        if row[x] and s is None:
            s = x
        elif not row[x] and s is not None:
            runs.append((s, x - 1))
            s = None
    if s is not None:
        runs.append((s, W - 1))
    runs = [r for r in runs if r[1] - r[0] > 20]
    print(f"检测到 {len(runs)} 个按键方框（中线 y={ym}）")
    for i, r in enumerate(runs[:9], 1):
        print(f"   键{i}: x={r[0]}..{r[1]}  宽={r[1]-r[0]+1}px")
    if not runs:
        return

    # ---- 3. 量字形：在第 5 个键的方框里找非 surface / 非 bg 的亮像素
    idx = 4 if len(runs) >= 5 else len(runs) // 2
    x0, x1 = runs[idx]
    box = a[band_top:band_bottom + 1, x0:x1 + 1]
    lum = box.mean(axis=2)
    glyph = lum > 110          # 数字与角标都是亮的，方框底(surface)亮度约 40
    ys, xs = np.where(glyph)
    if len(ys) == 0:
        print("键内未找到字形")
        return
    gh = ys.max() - ys.min() + 1
    gw = xs.max() - xs.min() + 1
    bh = band_bottom - band_top + 1
    bw = x1 - x0 + 1
    print(f"键内亮像素（数字+角标）范围: 高={gh}px 宽={gw}px")
    print(f"   相对键高占比: {gh / bh:.3f}   (方框 {bw}x{bh})")

    # ---- 4. 整体内容范围：找所有"非纯 bg"像素的行范围（排除状态栏）
    nonbg = ~bg
    colsum = nonbg.sum(axis=1)
    rows = np.where(colsum > W * 0.02)[0]
    print(f"非背景像素行范围: {rows.min()}..{rows.max()}")
    print(f"   下方留白 = {H - 1 - rows.max()}px，上方到状态栏内容起 = {rows.min()}px")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dist/device-shot.png")
