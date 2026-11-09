#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
生成 Android 启动图标（纯标准库，不依赖 Pillow）。

图形：深色圆角方块 + 3x3 数独网格，中心格用强调色填充。
用 4x 超采样后盒式降采样做抗锯齿。
"""
import os
import struct
import zlib

BG     = (0x16, 0x1A, 0x29)   # #161A29  classic 深色主题 bg
GRID   = (0xA6, 0xAE, 0xFF)   # #A6AEFF  entry
FILL   = (0x5B, 0x67, 0xE8)   # #5B67E8  accent

SS = 4          # 超采样倍数
RADIUS = 0.22   # 圆角半径（相对边长）
G0, G1 = 0.17, 0.83          # 网格区域
LW = 0.036                   # 线宽（相对边长）


def inside_round_rect(x, y, r):
    """x,y in 0..1"""
    if x < 0 or x > 1 or y < 0 or y > 1:
        return False
    cx = min(max(x, r), 1 - r)
    cy = min(max(y, r), 1 - r)
    dx, dy = x - cx, y - cy
    return dx * dx + dy * dy <= r * r


def sample(x, y):
    """返回该点的 (r,g,b,a)；a=0 表示透明"""
    if not inside_round_rect(x, y, RADIUS):
        return (0, 0, 0, 0)

    col = BG

    # 中心格填充（网格区域中间 1/3）
    span = G1 - G0
    c0 = G0 + span / 3.0
    c1 = G0 + span * 2.0 / 3.0
    if c0 <= x <= c1 and c0 <= y <= c1:
        col = FILL

    # 网格线 + 外框
    half = LW / 2.0
    on_grid = False
    # 外框
    for edge in (G0, G1):
        if abs(x - edge) <= half and G0 - half <= y <= G1 + half:
            on_grid = True
        if abs(y - edge) <= half and G0 - half <= x <= G1 + half:
            on_grid = True
    # 内部两条线
    for t in (1, 2):
        p = G0 + span * t / 3.0
        if abs(x - p) <= half and G0 - half <= y <= G1 + half:
            on_grid = True
        if abs(y - p) <= half and G0 - half <= x <= G1 + half:
            on_grid = True
    if on_grid:
        col = GRID

    return (col[0], col[1], col[2], 255)


def render(size):
    big = size * SS
    # 累积
    acc = [[0, 0, 0, 0] for _ in range(size * size)]
    for j in range(big):
        y = (j + 0.5) / big
        row_base = (j // SS) * size
        for i in range(big):
            x = (i + 0.5) / big
            r, g, b, a = sample(x, y)
            cell = acc[row_base + (i // SS)]
            if a:
                cell[0] += r * a
                cell[1] += g * a
                cell[2] += b * a
                cell[3] += a
            # 全透明像素不加权
    n = SS * SS
    out = bytearray()
    for cell in acc:
        if cell[3] == 0:
            out += b"\x00\x00\x00\x00"
        else:
            out += bytes((cell[0] // cell[3], cell[1] // cell[3], cell[2] // cell[3],
                          min(255, cell[3] // n)))
    return bytes(out)


def write_png(path, size, rgba):
    raw = bytearray()
    stride = size * 4
    for y in range(size):
        raw.append(0)                      # filter: None
        raw += rgba[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)
    print("  %-46s %4d x %4d  %6d bytes" % (path, size, size, len(png)))


if __name__ == "__main__":
    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res")
    # Android 各密度启动图标尺寸
    plan = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    print("生成启动图标：")
    for folder, px in plan.items():
        write_png(os.path.join(base, folder, "ic_launcher.png"), px, render(px))
    print("完成。")
