# -*- coding: utf-8 -*-
"""独立复核：用 Python 的 MRV 回溯统计每行题目的解数，验证唯一性。

输入 puzzles.txt（由 verify.exe 生成）：
    <81 位题面> <81 位答案> <难度> <C++ 判定解数>
输出：每个难度的统计 + 任何不一致的明细。
"""
import sys, collections

def popcount(x):
    return bin(x).count("1")

def solve_count(grid, limit=3, want_solution=False):
    """返回 (解数, 第一个解)。grid 会被原地修改，调用前自己 copy。"""
    rows = [0] * 9
    cols = [0] * 9
    box = [0] * 9
    for i in range(81):
        v = grid[i]
        if v:
            r, c = i // 9, i % 9
            b = (r // 3) * 3 + c // 3
            rows[r] |= 1 << v
            cols[c] |= 1 << v
            box[b] |= 1 << v

    cells = [i for i in range(81) if grid[i] == 0]
    cnt = 0
    first = None

    def rec():
        nonlocal cnt, first
        best = -1
        bestmask = 0
        bestn = 10
        for i in cells:
            if grid[i]:
                continue
            r, c = i // 9, i % 9
            used = rows[r] | cols[c] | box[(r // 3) * 3 + c // 3]
            n = 9 - popcount(used)
            if n < bestn:
                bestn = n
                best = i
                bestmask = used
                if n == 0:
                    return                       # 这个分支死了
        if best < 0:
            cnt += 1
            if first is None:
                first = grid[:]
            return

        r, c = best // 9, best % 9
        b = (r // 3) * 3 + c // 3
        for v in range(1, 10):
            bit = 1 << v
            if bestmask & bit:
                continue
            grid[best] = v
            rows[r] |= bit
            cols[c] |= bit
            box[b] |= bit
            rec()
            grid[best] = 0
            rows[r] &= ~bit
            cols[c] &= ~bit
            box[b] &= ~bit
            if cnt >= limit:
                return

    rec()
    return cnt, (first if want_solution else None)


def main(path="puzzles.txt", limit=3):
    stat = collections.defaultdict(lambda: {"n": 0, "sols": collections.Counter(),
                                            "bad": [], "sol_mismatch": 0})
    with open(path) as f:
        for ln, line in enumerate(f, 1):
            parts = line.split()
            if len(parts) < 4:
                continue
            pz, sol, diff, cpp_n = parts[0], parts[1], parts[2], int(parts[3])
            grid = [int(ch) for ch in pz]
            given = [int(ch) for ch in sol]
            n, one = solve_count(grid[:], limit=limit, want_solution=True)

            s = stat[diff]
            s["n"] += 1
            s["sols"][n] += 1
            if n != cpp_n:
                s["bad"].append((ln, n, cpp_n, pz))
            if n == 1 and one != given:
                s["sol_mismatch"] += 1
                s["bad"].append((ln, "solution-mismatch", 0, pz))

    print("=" * 66)
    for diff in ("easy", "medium", "hard", "expert"):
        if diff not in stat:
            continue
        s = stat[diff]
        dist = ", ".join(f"{k}解×{v}" for k, v in sorted(s["sols"].items()))
        print(f"{diff:8s} 局数={s['n']:<4d} 解数分布: {dist}")
        print(f"{'':8s} 与 C++ 判定不一致={len(s['bad'])}  答案与唯一解不符={s['sol_mismatch']}")
        for b in s["bad"][:3]:
            print("         明细:", b[0], "py=", b[1], "cpp=", b[2], b[3])
    bad_total = sum(len(s["bad"]) for s in stat.values())
    print("=" * 66)
    print("结论:", "全部唯一解，两套实现一致" if bad_total == 0 else f"发现 {bad_total} 处不一致")
    return 0 if bad_total == 0 else 1


if __name__ == "__main__":
    p = sys.argv[1] if len(sys.argv) > 1 else "puzzles.txt"
    sys.exit(main(p))
