# -*- coding: utf-8 -*-
"""数独「笔记小数字」讲解图 2：左边真实渲染，右边位置模板，说明落位规则。"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
shot = Image.open(os.path.join(HERE, "notes.png")).convert("RGB")

BG    = (22, 26, 41)
CARD  = (16, 19, 32)
ACC   = (91, 103, 232)
ACC_L = (166, 174, 255)
TXT   = (238, 240, 248)
HINT  = (138, 144, 170)
DIM   = (58, 64, 96)

F = lambda s, b=False: ImageFont.truetype(
    r"C:\Windows\Fonts\msyhbd.ttc" if b else r"C:\Windows\Fonts\msyh.ttc", s)

W, CH = 540, 486
im = Image.new("RGB", (W, CH), BG)
d = ImageDraw.Draw(im)

d.text((24, 22), "小数字的落位规则", font=F(19, True), fill=TXT)
d.text((24, 50), "格内 3×3 九个位置固定对应 1–9，同一数字永远在同一处", font=F(13), fill=HINT)

SC = shot.width / 432.0
CELL = (400 - 4) / 9.0
G = 226
x0, y0 = 24, 104

idx = dict(kv.split("=") for kv in
           open(os.path.join(HERE, "bin", "notesidx.txt")).read().split())
IA = int(idx["a"])
R, C = IA // 9, IA % 9
cw = int(round(CELL * SC))
px0 = int(round((18 + C * CELL) * SC))
py0 = int(round((82 + R * CELL) * SC))
real = shot.crop((px0, py0, px0 + cw, py0 + cw)).resize((G, G), Image.NEAREST)
im.paste(real, (x0, y0))
d.rounded_rectangle((x0 - 1, y0 - 1, x0 + G, y0 + G), radius=4, outline=ACC, width=2)
d.text((x0, y0 - 22), "实际显示", font=F(12), fill=HINT)

BX, BY, S = 290, 104, G
d.text((BX, BY - 22), "位置含义", font=F(12), fill=HINT)
cell = S / 3
have = {1, 3, 5, 7, 9}
for n in range(1, 10):
    r, c = (n - 1) // 3, (n - 1) % 3
    bx, by = BX + c * cell, BY + r * cell
    on = n in have
    d.rounded_rectangle((bx + 1, by + 1, bx + cell - 2, by + cell - 2), radius=3,
                        fill=(29, 35, 64) if on else CARD,
                        outline=ACC if on else (34, 40, 66), width=2 if on else 1)
    f = F(24, True)
    tw = d.textlength(str(n), font=f)
    d.text((bx + (cell - tw) / 2, by + cell / 2 - 16), str(n), font=f,
           fill=ACC_L if on else DIM)

ly = BY + G + 28
d.rounded_rectangle((24, ly, W - 24, ly + 104), radius=8, fill=CARD)
d.text((40, ly + 14), "左上=1   正上=2   右上=3", font=F(14, True), fill=ACC_L)
d.text((40, ly + 42), "左中=4   正中=5   右中=6", font=F(14, True), fill=ACC_L)
d.text((40, ly + 70), "左下=7   正下=8   右下=9", font=F(14, True), fill=ACC_L)

out = os.path.join(HERE, "notes_layout.png")
im.save(out)
print("saved", out, im.size)
