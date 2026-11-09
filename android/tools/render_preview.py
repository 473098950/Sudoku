#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
按 SudokuView 的同一套公式与配色，在真机像素下渲染一张布局预览图。

注意：这不是手机截图，而是用 Python 复现同一份几何/配色规则得到的
「设计核算图」，用于确认比例、留白、字号是否合适。
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "dist", "android-preview-1216x2640.png")

# ---------------------------------------------------------------- 机型
W, H = 1216, 2640
DPI = 480
INSET_TOP, INSET_BOTTOM = 96, 78
f = DPI / 160.0

# ---------------------------------------------------------------- classic 深色调色板
BG      = (0x16, 0x1A, 0x29)
SURFACE = (0x21, 0x26, 0x3A)
TEXT    = (0xEE, 0xF0, 0xF8)
HINT    = (0x8A, 0x90, 0xAA)
ACCENT  = (0x5B, 0x67, 0xE8)
ENTRY   = (0xA6, 0xAE, 0xFF)
MISTAKE = (0xFF, 0x6B, 0x72)
AREA, SAME, SEL, CONF = 0.12, 0.26, 0.40, 0.20


def mix(a, b, t):
    return tuple(int(round(a[i] * t + b[i] * (1 - t))) for i in range(3))


C_AREA = mix(ACCENT, BG, AREA)
C_SAME = mix(ACCENT, BG, SAME)
C_SEL  = mix(ACCENT, BG, SEL)
C_DUP  = mix(MISTAKE, BG, CONF)

# ---------------------------------------------------------------- 字体
CJK = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 1)     # 占位，下面按需实例化
_cjk_cache, _dig_cache = {}, {}


def cjk(size, bold=False):
    key = (int(size), bold)
    if key not in _cjk_cache:
        path = r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc"
        _cjk_cache[key] = ImageFont.truetype(path, max(1, int(size)))
    return _cjk_cache[key]


def digit(size, bold=False):
    key = (int(size), bold)
    if key not in _dig_cache:
        path = r"C:\Windows\Fonts\segoeuib.ttf" if bold else r"C:\Windows\Fonts\segoeui.ttf"
        _dig_cache[key] = ImageFont.truetype(path, max(1, int(size)))
    return _dig_cache[key]


# ---------------------------------------------------------------- 布局（同 computeLayout）
pad      = 14 * f
availH   = H - INSET_TOP - INSET_BOTTOM
board    = min(W - 2 * pad, availH * 0.62)
u        = board / 400.0
TB1H, GAP1, TB2H, GAPB, KEYH, GAPK = 34 * f, 12 * f, 44 * f, 22 * f, 68 * f, 18 * f
contentH = TB1H + GAP1 + TB2H + GAPB + board + GAPK + KEYH
top      = INSET_TOP + max(10 * f, (availH - contentH) * 0.42)

cx = W / 2.0
tb1_cy = top + TB1H / 2
tb2_cy = top + TB1H + GAP1 + TB2H / 2
bd_top = top + TB1H + GAP1 + TB2H + GAPB
bd_l   = cx - board / 2
bd_r   = cx + board / 2
kp_top = bd_top + board + GAPK

border  = max(1 * f, 2.0 * u)
cw      = (board - 2 * border) / 9.0
digit_sz = 20.2 * 1.3333 * u
note_sz  = 8.0 * 1.3333 * u

kgap  = 4 * f
kw    = (board - kgap * 8) / 9.0
kdig  = KEYH * 0.46

# ---------------------------------------------------------------- 棋局示例
PUZZLE = [
    5, 3, 0, 0, 7, 0, 0, 0, 0,
    6, 0, 0, 1, 9, 5, 0, 0, 0,
    0, 9, 8, 0, 0, 0, 0, 6, 0,
    8, 0, 0, 0, 6, 0, 0, 0, 3,
    4, 0, 0, 8, 0, 3, 0, 0, 1,
    7, 0, 0, 0, 2, 0, 0, 0, 6,
    0, 6, 0, 0, 0, 0, 2, 8, 0,
    0, 0, 0, 4, 1, 9, 0, 0, 5,
    0, 0, 0, 0, 8, 0, 0, 7, 9,
]
SOLUTION = [
    5, 3, 4, 6, 7, 8, 9, 1, 2,
    6, 7, 2, 1, 9, 5, 3, 4, 8,
    1, 9, 8, 3, 4, 2, 5, 6, 7,
    8, 5, 9, 7, 6, 1, 4, 2, 3,
    4, 2, 6, 8, 5, 3, 7, 9, 1,
    7, 1, 3, 9, 2, 4, 8, 5, 6,
    9, 6, 1, 5, 3, 7, 2, 8, 4,
    2, 8, 7, 4, 1, 9, 6, 3, 5,
    3, 4, 5, 2, 8, 6, 1, 7, 9,
]
VALUES = list(PUZZLE)
VALUES[2] = 4       # 正确填入
VALUES[5] = 8
VALUES[30] = 5      # 正确填入
VALUES[3] = 5       # 错误 + 与 (0,0) 的 5 冲突，且被选中
NOTES = {}
NOTES[10] = [2, 5, 7]
NOTES[46] = [1, 8]
SEL_IDX = 3
MISTAKES = 2
TIME_TEXT = "00:02"

# ---------------------------------------------------------------- 绘制工具
page = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(page)


def glyph(name, cxp, cyp, box, color, width_scale=1.0):
    """在 24x24 空间里画图标，整体缩放到 box"""
    s = box / 24.0
    lw = max(2, int(round(1.9 / s * box / box * 1.0 * max(1, s) * 1.05)))
    lw = max(2, int(round(1.9 * s)))
    def P(x, y):
        return (cxp - box / 2 + x * s, cyp - box / 2 + y * s)

    if name == "clock":
        d.ellipse([P(3, 3), P(21, 21)], outline=color, width=lw)
        d.line([P(12, 7.5), P(12, 12)], fill=color, width=lw)
        d.line([P(12, 12), P(15, 13.8)], fill=color, width=lw)
    elif name == "cross":
        d.line([P(6, 6), P(18, 18)], fill=color, width=lw)
        d.line([P(18, 6), P(6, 18)], fill=color, width=lw)
    elif name == "undo":
        d.line([P(3, 8), P(3, 13), P(8, 13)], fill=color, width=lw, joint="curve")
        d.arc([P(3.5, 3.5), P(20, 19)], 200, 20, fill=color, width=lw)
    elif name == "pencil":
        d.line([P(12, 20.4), P(21, 20.4)], fill=color, width=lw)
        d.line([P(16.6, 3.6), P(20.4, 7.4), P(7, 20.8), P(3, 21), P(3.2, 17), P(16.6, 3.6)],
               fill=color, width=lw, joint="curve")
        d.line([P(14.2, 6), P(18, 9.8)], fill=color, width=lw)
    elif name == "eraser":
        d.line([P(8, 20.4), P(20, 20.4)], fill=color, width=lw)
        d.line([P(4.6, 15.4), P(11.4, 8.6)], fill=color, width=lw)
        d.line([P(17.6, 12), P(12.8, 19.2), P(7.3, 19.2), P(4.6, 15.4)], fill=color,
               width=lw, joint="curve")
        d.line([P(8.2, 11.8), P(13.8, 17.4)], fill=color, width=lw)
    elif name == "bulb":
        d.line([P(9.2, 18), P(14.8, 18)], fill=color, width=lw)
        d.line([P(10.3, 21), P(13.7, 21)], fill=color, width=lw)
        d.arc([P(6, 3), P(18, 16.2)], 150, 390, fill=color, width=lw)
        d.line([P(9.4, 15.9), P(14.6, 15.9)], fill=color, width=lw)
    elif name == "plus":
        d.line([P(12, 5), P(12, 19)], fill=color, width=lw)
        d.line([P(5, 12), P(19, 12)], fill=color, width=lw)
    elif name == "palette":
        d.ellipse([P(3, 3), P(21, 21)], outline=color, width=lw)
        for pt in ((9.2, 9.2), (14.8, 9.2), (9.6, 14.6)):
            r = 1.15 * s
            x, y = P(*pt)
            d.ellipse([x - r, y - r, x + r, y + r], fill=color)
    elif name == "chevron":
        d.line([P(6.5, 9.5), P(12, 15), P(17.5, 9.5)], fill=color, width=lw, joint="curve")


def text_mm(x, y, s, font, fill):
    d.text((x, y), s, font=font, fill=fill, anchor="mm")


# ---------------------------------------------------------------- 第一行
def pill(x, cy, icon, label, bad):
    fnt = cjk(13 * f, bold=True)
    tw = d.textlength(label, font=fnt)
    icon_box, icon_x = 15 * f, 11 * f
    text_x = icon_x + icon_box + 6 * f
    w = text_x + tw + 13 * f
    h = 34 * f
    d.rounded_rectangle([x, cy - h / 2, x + w, cy + h / 2], radius=12 * f, fill=SURFACE)
    glyph(icon, x + icon_x + icon_box / 2, cy, icon_box, MISTAKE if bad else HINT)
    text_mm(x + text_x + tw / 2, cy, label, fnt, MISTAKE if bad else TEXT)
    return x + w


x = pad
x = pill(x, tb1_cy, "clock", TIME_TEXT, False) + 8 * f
x = pill(x, tb1_cy, "cross", str(MISTAKES), True)

# 难度按钮（右对齐）
diff_w, diff_h = 92 * f, 34 * f
dl = W - pad - diff_w
d.rounded_rectangle([dl, tb1_cy - diff_h / 2, dl + diff_w, tb1_cy + diff_h / 2],
                    radius=12 * f, fill=SURFACE)
fnt = cjk(13 * f, bold=True)
text_mm(dl + 14 * f + d.textlength("中等", font=fnt) / 2, tb1_cy, "中等", fnt, TEXT)
glyph("chevron", dl + diff_w - 18 * f, tb1_cy, 13 * f, HINT)

# ---------------------------------------------------------------- 第二行：图标
ICONS = ["undo", "pencil", "eraser", "bulb", "plus", "palette"]
isz = 44 * f
igap = min(14 * f, (board - 6 * isz) / 5)
row_w = 6 * isz + 5 * igap
ix = (W - pad) - row_w
for i, name in enumerate(ICONS):
    box = [ix, tb2_cy - isz / 2, ix + isz, tb2_cy + isz / 2]
    if name == "pencil":                       # 笔记模式开启态
        d.rounded_rectangle(box, radius=13 * f, fill=C_SAME)
        glyph(name, ix + isz / 2, tb2_cy, isz * 0.61, ENTRY)
    else:
        col = HINT
        if name == "undo":                     # 无可撤销 → 变淡
            col = tuple(int(HINT[c] * 0.135 + BG[c] * 0.865) for c in range(3))
            glyph(name, ix + isz / 2, tb2_cy, isz * 0.61, col)
        else:
            glyph(name, ix + isz / 2, tb2_cy, isz * 0.61, col)
    ix += isz + igap

# ---------------------------------------------------------------- 棋盘
bd_w = int(round(board))
bd = Image.new("RGBA", (bd_w, bd_w), (0, 0, 0, 0))
db = ImageDraw.Draw(bd)

bad = set()
for i in range(81):
    v = VALUES[i]
    if not v:
        continue
    r, c = divmod(i, 9)
    for k in range(9):
        a, bb = r * 9 + k, k * 9 + c
        if a != i and VALUES[a] == v:
            bad.add(i); bad.add(a)
        if bb != i and VALUES[bb] == v:
            bad.add(i); bad.add(bb)
    br, bc = r - r % 3, c - c % 3
    for xx in range(3):
        for yy in range(3):
            p = (br + xx) * 9 + bc + yy
            if p != i and VALUES[p] == v:
                bad.add(i); bad.add(p)

si = SEL_IDX
sv = VALUES[si]
sr, sc = divmod(si, 9)
sbr, sbc = sr - sr % 3, sc - sc % 3

for i in range(81):
    r, c = divmod(i, 9)
    x0 = border + c * cw
    y0 = border + r * cw
    rect = [x0, y0, x0 + cw, y0 + cw]
    v = VALUES[i]
    is_sel = i == si
    is_peer = (r == sr or c == sc or
               (sbr <= r < sbr + 3 and sbc <= c < sbc + 3))

    fill = None
    if sv:
        if v == sv:
            fill = C_SAME
        elif v == 0 and is_peer:
            fill = C_AREA
    elif is_peer and not is_sel:
        fill = C_AREA
    if i in bad:
        fill = C_DUP
    if is_sel:
        fill = C_SEL
    if fill:
        db.rectangle(rect, fill=fill)

    if v:
        if PUZZLE[i]:
            col, bold = TEXT, True
        elif v == SOLUTION[i]:
            col, bold = ENTRY, False
        else:
            col, bold = MISTAKE, False
        font = digit(digit_sz, bold=bold)
        dim = sv and v != sv and not is_sel
        if dim:
            col = tuple(int(col[k] * 0.40 + (fill or BG)[k] * 0.60) for k in range(3))
        db.text((x0 + cw / 2, y0 + cw / 2), str(v), font=font, fill=col, anchor="mm")
    elif i in NOTES:
        nf = digit(note_sz)
        for nd in NOTES[i]:
            nx = x0 + ((nd - 1) % 3) * cw / 3
            ny = y0 + ((nd - 1) // 3) * cw / 3
            db.text((nx + cw / 6, ny + cw / 6), str(nd), font=nf, fill=HINT, anchor="mm")

# 网格线（带 alpha，叠一层）
ov = Image.new("RGBA", (bd_w, bd_w), (0, 0, 0, 0))
do = ImageDraw.Draw(ov)
for i in range(1, 9):
    rule = (i % 3 == 0)
    a = 0.62 if rule else 0.18
    wl = max(2, int(round((2.0 if rule else 1.0) * u)))
    col = TEXT + (int(255 * a),)
    px = border + i * cw
    do.line([(px, border), (px, bd_w - border)], fill=col, width=wl)
    do.line([(border, px), (bd_w - border, px)], fill=col, width=wl)
bd = Image.alpha_composite(bd, ov)

# 选中描边
db = ImageDraw.Draw(bd)
ins = max(1.2, 0.8 * u)
rx = border + sc * cw + ins
ry = border + sr * cw + ins
db.rounded_rectangle([rx, ry, rx + cw - 2 * ins, ry + cw - 2 * ins],
                     radius=max(2.0, 2.0 * u), outline=ACCENT,
                     width=max(3, int(round(1.6 * u))))

# 圆角遮罩
mask = Image.new("L", (bd_w, bd_w), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, bd_w - 1, bd_w - 1],
                                       radius=max(2.0, 8.0 * u), fill=255)
page.paste(bd, (int(round(bd_l)), int(round(bd_top))), mask)

# 外框
d.rounded_rectangle([bd_l + 1, bd_top + 1, bd_r - 1, bd_top + board - 1],
                    radius=max(2.0, 8.0 * u),
                    outline=tuple(int(TEXT[k] * 0.62 + BG[k] * 0.38) for k in range(3)),
                    width=max(2, int(round(border))))

# ---------------------------------------------------------------- 数字键盘
remaining = {dd: 9 - VALUES.count(dd) for dd in range(1, 10)}
for dd in range(1, 10):
    kx = bd_l + (dd - 1) * (kw + kgap)
    box = [kx, kp_top, kx + kw, kp_top + KEYH]
    done = remaining[dd] <= 0
    d.rounded_rectangle(box, radius=min(kw, KEYH) * 0.24, fill=SURFACE)
    col = HINT if done else ENTRY
    if done:
        col = tuple(int(HINT[k] * 0.28 + SURFACE[k] * 0.72) for k in range(3))
    text_mm(kx + kw / 2, kp_top + KEYH / 2 - KEYH * 0.06, str(dd),
            digit(kdig), col)
    if 0 < remaining[dd] < 9:
        d.text((kx + kw - kw * 0.16, kp_top + kdig * 0.16), str(remaining[dd]),
               font=digit(KEYH * 0.20), fill=HINT, anchor="ra")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
page.save(OUT)
print("已生成:", os.path.abspath(OUT), "%dx%d" % page.size)
