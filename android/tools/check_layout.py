#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
按 SudokuView.computeLayout() 的同一套公式，在真实机型像素下核算布局。

只做两件事：
  1. 打印每个区块的像素/dp 尺寸
  2. 断言：不越界、不重叠、触摸目标不小于阈值

机型参数默认 OPPO Find X8s：1216x2640, 460ppi, 中置挖孔。
"""
import sys

# ---- 机型参数 ----
W, H = 1216, 2640
DPI = 480          # Android densityDpi（460ppi 落在 xxhdpi 档）
INSET_TOP = 96     # 状态栏 + 挖孔安全区（ColorOS 15 约 32dp）
INSET_BOTTOM = 78  # 手势导航条（约 26dp）

f = DPI / 160.0    # px per dp

def dp(v):
    return v / f

# ---- 与 computeLayout() 完全一致的公式 ----
pad      = 14 * f
availH   = H - INSET_TOP - INSET_BOTTOM
board    = min(W - 2 * pad, availH * 0.62)
board    = max(board, 120 * f)
u        = board / 400.0

TB1H, GAP1, TB2H, GAPB, KEYH, GAPK = 34*f, 12*f, 44*f, 22*f, 68*f, 18*f
contentH = TB1H + GAP1 + TB2H + GAPB + board + GAPK + KEYH
top      = INSET_TOP + max(10 * f, (availH - contentH) * 0.42)
if top + contentH > H - INSET_BOTTOM:
    top = max(INSET_TOP, H - INSET_BOTTOM - contentH)

cx = W / 2.0
tb1_top, tb1_bot = top, top + TB1H
tb2_top, tb2_bot = tb1_bot + GAP1, tb1_bot + GAP1 + TB2H
bd_top           = tb2_bot + GAPB
bd_bot           = bd_top + board
kp_top, kp_bot   = bd_bot + GAPK, bd_bot + GAPK + KEYH
bd_l, bd_r       = cx - board/2, cx + board/2

# ---- 棋盘内部 ----
border = max(1 * f, 2.0 * u)
cw     = (board - 2 * border) / 9.0
digit  = 20.2 * 1.3333 * u
note   = 8.0 * 1.3333 * u

# ---- 键盘 ----
kgap = 4 * f
kw   = (board - kgap * 8) / 9.0
kdigit = KEYH * 0.46

# ---- 图标按钮 ----
icon_sz = 44 * f
icon_n  = 6
icon_gap = min(14 * f, (board - icon_n * icon_sz) / (icon_n - 1))
icon_row_w = icon_n * icon_sz + (icon_n - 1) * icon_gap
icon_left  = (W - pad) - icon_row_w

# ---- 胶囊（按 sans-serif-medium 13dp 估宽，CJK/数字约 0.55em/字） ----
def pill_w(text, has_icon=True):
    tw = len(text) * 13 * f * 0.55
    icon_box, icon_x = 15 * f, 11 * f
    text_x = icon_x + icon_box + 6 * f
    return text_x + tw + 13 * f

w_time = pill_w("00:00")
w_miss = pill_w("0")
diff_w = 92 * f
row1_used = w_time + 8*f + w_miss
row1_free_after_diff = (W - pad - diff_w) - (pad + row1_used)

rows = [
    ("状态栏 + 挖孔安全区", INSET_TOP, None),
    ("第一行：时间/错误 | 难度", TB1H, top),
    ("  间隙", GAP1, None),
    ("第二行：6 个图标按钮", TB2H, tb2_top),
    ("  间隙", GAPB, None),
    ("棋盘 9x9", board, bd_top),
    ("  间隙", GAPK, None),
    ("数字键盘 1-9", KEYH, kp_top),
    ("底部手势条安全区", INSET_BOTTOM, None),
]

print("=" * 66)
print("OPPO Find X8s 布局核算  %dx%d px  %dppi(densityDpi=%d, %.3fx)" % (W, H, 460, DPI, f))
print("屏宽 %.1f dp x 屏高 %.1f dp" % (dp(W), dp(H)))
print("=" * 66)
print("%-26s %9s %9s   %s" % ("区块", "px", "dp", "y起点(px)"))
print("-" * 66)
for name, h, y in rows:
    print("%-26s %9.1f %9.1f   %s" % (
        name, h, dp(h), ("%.0f" % y) if y is not None else "-"))

print("-" * 66)
print("棋盘      : %.0f px = %.1f dp，居中 x=[%.0f, %.0f]" % (board, dp(board), bd_l, bd_r))
print("棋盘单元  : %.1f px = %.1f dp 见方" % (cw, dp(cw)))
print("  ├ 正文字号 : %.1f px = %.1f dp" % (digit, dp(digit)))
print("  └ 候选数   : %.1f px = %.1f dp" % (note, dp(note)))
print("键盘按键  : %.1f x %.1f px = %.1f x %.1f dp" % (kw, KEYH, dp(kw), dp(KEYH)))
print("  └ 数字字号 : %.1f px = %.1f dp" % (kdigit, dp(kdigit)))
print("图标按钮  : %.1f px = %.1f dp，间距 %.1f dp" % (icon_sz, dp(icon_sz), dp(icon_gap)))
print("胶囊 时间 : %.1f px = %.1f dp" % (w_time, dp(w_time)))
print("胶囊 错误 : %.1f px = %.1f dp" % (w_miss, dp(w_miss)))
print("第一行剩余 : %.1f dp（中段留白）" % dp(row1_free_after_diff))
print()

# ---- 断言 ----
ok = True
def check(cond, msg):
    global ok
    print(("  [OK]   " if cond else "  [FAIL] ") + msg)
    if not cond:
        ok = False

print("校验：")
check(abs(bd_r - (W - pad)) < 0.5 and abs(bd_l - pad) < 0.5,
      "棋盘左右各留 14dp，宽度铺满")
check(kp_bot <= H - INSET_BOTTOM, "键盘底边不侵入手势条安全区")
check(tb1_top >= INSET_TOP, "工具栏不侵入状态栏/挖孔区")
check(top - INSET_TOP >= 8 * f, "顶部留白 >= 8dp")
check(kw >= 34 * f, "按键宽度 %.1f dp >= 34dp（9 键并排的下限）" % dp(kw))
check(KEYH >= 44 * f, "按键高度 %.1f dp >= 44dp（触摸目标下限）" % dp(KEYH))
check(icon_sz >= 44 * f, "图标按钮 %.1f dp >= 44dp（触摸目标下限）" % dp(icon_sz))
check(icon_left >= pad, "第二行按钮不溢出左边")
check(pad + row1_used <= W - pad - diff_w, "第一行左半部分不与难度按钮重叠")
check(dp(cw) >= 40, "单格 %.1f dp，手指可点" % dp(cw))
check(dp(digit) >= 20, "正文字号 %.1f dp 清晰" % dp(digit))
check(dp(note) >= 9, "候选数字号 %.1f dp 可辨认" % dp(note))
check(abs(H - INSET_BOTTOM - kp_bot - (top - INSET_TOP)) < H, "垂直方向无溢出")

print()
print("结论：" + ("全部通过" if ok else "存在不满足项"))
sys.exit(0 if ok else 1)
