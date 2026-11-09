"""量出棋盘外框矩形，据此推算格子中心坐标，供 adb input tap 使用。"""
import sys
import numpy as np
from PIL import Image

BOARD_BORDER = (155, 157, 167)   # alpha(text,0.62) over bg
TOL = 22


def main(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int16)
    m = (np.abs(a[:, :, 0] - BOARD_BORDER[0]) <= TOL) & \
        (np.abs(a[:, :, 1] - BOARD_BORDER[1]) <= TOL) & \
        (np.abs(a[:, :, 2] - BOARD_BORDER[2]) <= TOL)
    ys, xs = np.where(m)
    if len(ys) == 0:
        print("未找到棋盘外框")
        return 1
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    print(f"棋盘外框: x={x0}..{x1} ({x1-x0+1}px)  y={y0}..{y1} ({y1-y0+1}px)")

    border = max(1.0, 2.0 * ((x1 - x0 + 1) / 400.0))
    cw = ((x1 - x0 + 1) - 2 * border) / 9.0
    ch = ((y1 - y0 + 1) - 2 * border) / 9.0
    print(f"cell={cw:.1f}x{ch:.1f}  border={border:.1f}")

    def cell(r, c):
        return (round(x0 + border + (c + 0.5) * cw),
                round(y0 + border + (r + 0.5) * ch))

    for (r, c) in [(0, 1), (4, 4), (8, 8)]:
        x, y = cell(r, c)
        print(f"cell({r},{c}) 中心 = ({x}, {y})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "dist/device-shot2.png"))
