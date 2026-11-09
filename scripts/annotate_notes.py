# -*- coding: utf-8 -*-
"""数独「笔记（候选数）」讲解图 1：整窗截图 + ①②③ 标注 + 底部图例。

用法：先在 bin 里跑
    SUDOKU_DUMP=1 ./Sudoku.exe --demo-notes --scale 1.0 --shot ../notes.png
然后 python annotate_notes.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOT = os.path.join(HERE, "notes.png")

BG    = (22, 26, 41)
BG2   = (16, 19, 32)
ACC   = (91, 103, 232)
ACC_L = (166, 174, 255)
TXT   = (238, 240, 248)
HINT  = (138, 144, 170)
GOLD  = (255, 209, 102)

F = lambda s, b=False: ImageFont.truetype(
    r"C:\Windows\Fonts\msyhbd.ttc" if b else r"C:\Windows\Fonts\msyh.ttc", s)

shot = Image.open(SHOT).convert("RGB")
W, H = shot.size                      # 540x685（逻辑 432x548 @125%）
SC = W / 432.0                        # 物理/设计

def drect(x, y, w, h, pad=5):
    return (x * SC - pad, y * SC - pad, (x + w) * SC + pad, (y + h) * SC + pad)

CELL = (400 - 4) / 9.0                # 棋盘内缩 2px 描边后每格 44
def cell_rect(idx, pad=5):
    r, c = idx // 9, idx % 9
    return drect(18 + c * CELL, 82 + r * CELL, CELL, CELL, pad)

idx = dict(kv.split("=") for kv in
           open(os.path.join(HERE, "bin", "notesidx.txt")).read().split())
A = cell_rect(int(idx["a"]))          # 笔记 1/3/5/7/9
B = cell_rect(int(idx["b"]))          # 笔记 2/4/8
PENCIL = drect(260, 41, 28, 28, 6)    # 铅笔按钮

LEG_H = 240
canvas = Image.new("RGB", (W, H + LEG_H), BG)
canvas.paste(shot, (0, 0))
d = ImageDraw.Draw(canvas)

def badge(xy, text, color=ACC):
    cx, cy = xy
    r = 11
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)
    f = F(13, True)
    tw = d.textlength(text, font=f)
    d.text((cx - tw / 2, cy - 9), text, font=f, fill=(255, 255, 255))

def mark(rect, label, color=ACC, pos="top"):
    d.rounded_rectangle(rect, radius=6, outline=color, width=2)
    if pos == "top":
        badge(((rect[0] + rect[2]) / 2, rect[1]), label, color)
    elif pos == "right":
        badge((rect[2], (rect[1] + rect[3]) / 2), label, color)
    else:
        badge((rect[2] + 4, rect[1] - 4), label, color)

mark(A, "1", ACC, "top")
mark(B, "2", ACC, "right")
mark(PENCIL, "3", GOLD, "out")

y0 = H
d.rectangle((0, y0, W, y0 + LEG_H), fill=BG2)
d.rectangle((0, y0, W, y0 + 2), fill=ACC)

rows = [
    ("1", ACC,  "小数字 = 候选数（笔记）",
     "表示「这格可能填 1 / 3 / 5 / 7 / 9」，不是答案，也不计入错误次数。"),
    ("2", ACC,  "每个数字的位置是固定的",
     "九宫格式排布：1 2 3 / 4 5 6 / 7 8 9，扫一眼就知道这格排除了哪几个数。"),
    ("3", GOLD, "铅笔图标点亮 = 笔记模式（快捷键 N）",
     "亮着时按数字只是打勾/取消候选；关掉后按数字才是真正填入答案。"),
]

ty = y0 + 22
for num, col, title, desc in rows:
    badge((30, ty + 12), num, col)
    d.text((54, ty), title, font=F(16, True), fill=TXT)
    d.text((54, ty + 25), desc, font=F(13), fill=HINT)
    ty += 72

out = os.path.join(HERE, "notes_explained.png")
canvas.save(out)
print("saved", out, canvas.size)
