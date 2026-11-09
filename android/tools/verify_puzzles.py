#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
用 Python 独立 MRV 求解器复核安卓版生成的题目：
  1. 答案本身是合法完整盘面
  2. 题面是答案的子集
  3. 题面恰好只有 1 个解，且与程序给的答案一致

不依赖 Java 侧的任何判断，避免"自己验自己"。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "verify", "check_unique.py")


def load_solver():
    ns = {}
    with open(SRC, encoding="utf-8") as f:
        exec(compile(f.read(), SRC, "exec"), ns)
    return ns["solve_count"]


def valid_full(grid):
    """完整盘面：每行/列/宫都是 1..9"""
    for i in range(9):
        if sorted(grid[i * 9:(i + 1) * 9]) != list(range(1, 10)):
            return False
        if sorted(grid[i + 9 * k] for k in range(9)) != list(range(1, 10)):
            return False
    for br in range(0, 9, 3):
        for bc in range(0, 9, 3):
            cells = [grid[(br + x) * 9 + bc + y] for x in range(3) for y in range(3)]
            if sorted(cells) != list(range(1, 10)):
                return False
    return True


def main():
    path = os.path.join(HERE, "..", "android_puzzles.txt")
    solve_count = load_solver()

    n = 0
    bad = 0
    per_diff = {}
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        p, s, diff, ms = line.split()
        puz = [int(ch) for ch in p]
        sol = [int(ch) for ch in s]
        n += 1
        st = per_diff.setdefault(diff, {"n": 0, "bad": 0, "givens": []})
        st["n"] += 1
        st["givens"].append(sum(1 for x in puz if x))

        problems = []
        if not valid_full(sol):
            problems.append("答案不是合法完整盘面")
        if any(puz[i] and puz[i] != sol[i] for i in range(81)):
            problems.append("题面与答案不一致")

        cnt, first = solve_count(list(puz), limit=3, want_solution=True)
        if cnt != 1:
            problems.append("解数 = %d（应为 1）" % cnt)
        elif first is not None and list(first) != sol:
            problems.append("唯一解与给定答案不同")

        if problems:
            bad += 1
            st["bad"] += 1
            print("  [FAIL] %s %s" % (diff, "; ".join(problems)))

    print("=" * 58)
    for d in ("easy", "medium", "hard", "expert"):
        if d not in per_diff:
            continue
        st = per_diff[d]
        g = st["givens"]
        print("  %-7s %3d 局   给数 %2d..%2d   异常 %d" % (d, st["n"], min(g), max(g), st["bad"]))
    print("=" * 58)
    print("共 %d 局，%d 局异常" % (n, bad))
    print("UNIQUE_OK" if bad == 0 else "UNIQUE_FAIL")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
