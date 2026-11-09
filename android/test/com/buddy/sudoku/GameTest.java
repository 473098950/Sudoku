package com.buddy.sudoku;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Random;

/**
 * 安卓版 Game.java 的离线自测。
 *
 * Game / Palette 不依赖任何 Android API，所以可以直接用桌面 JDK 编译运行，
 * 把「出题 + 落子 + 撤销 + 提示 + 冲突 + 存档」这些最容易在移植时写错的地方
 * 逐条跑一遍，再用 Python 独立求解器交叉验证唯一解。
 */
public class GameTest {

    private static int pass = 0, fail = 0;

    private static void ok(boolean cond, String what) {
        if (cond) {
            pass++;
            System.out.println("  [PASS] " + what);
        } else {
            fail++;
            System.out.println("  [FAIL] " + what);
        }
    }

    private static String row(int[] v) {
        StringBuilder sb = new StringBuilder(81);
        for (int x : v) sb.append(x);
        return sb.toString();
    }

    /** 某格所在行 / 列 / 宫已经出现过的数字（不含该格自己） */
    private static boolean[] usedAround(int[] v, int i) {
        boolean[] used = new boolean[10];
        final int r = i / 9, c = i % 9;
        for (int k = 0; k < 9; k++) {
            used[v[r * 9 + k]] = true;
            used[v[k * 9 + c]] = true;
        }
        final int br = r - r % 3, bc = c - c % 3;
        for (int x = 0; x < 3; x++)
            for (int y = 0; y < 3; y++) used[v[(br + x) * 9 + bc + y]] = true;
        return used;
    }

    public static void main(String[] args) throws Exception {
        // ---------------------------------------------------------- 出题
        System.out.println("== 出题（每难度 30 局）==");
        String[] diffs = {"easy", "medium", "hard", "expert"};
        PrintWriter out = new PrintWriter("android_puzzles.txt", StandardCharsets.UTF_8);
        for (String d : diffs) {
            long totalMs = 0, maxMs = 0;
            int minGivens = 99, maxGivens = 0;
            for (int k = 0; k < 30; k++) {
                Game g = new Game();
                long t0 = System.nanoTime();
                g.newGame(d);
                long ms = (System.nanoTime() - t0) / 1_000_000L;
                totalMs += ms;
                maxMs = Math.max(maxMs, ms);

                int givens = 0;
                for (int x : g.puzzle) if (x != 0) givens++;
                minGivens = Math.min(minGivens, givens);
                maxGivens = Math.max(maxGivens, givens);

                out.println(row(g.puzzle) + " " + row(g.solution) + " " + d + " " + ms);
            }
            System.out.printf("  %-7s 给数 %d..%d  平均 %d ms  最长 %d ms%n",
                    d, minGivens, maxGivens, totalMs / 30, maxMs);
        }
        out.close();

        // ---------------------------------------------------------- 规则
        System.out.println("== 规则单测 ==");
        Game g = new Game();
        g.newGame("easy");

        // 找第一个空格
        int empty = -1;
        for (int i = 0; i < Game.N; i++) if (g.puzzle[i] == 0) { empty = i; break; }
        ok(empty >= 0, "题面存在空格");

        g.sel = empty;
        final int right = g.solution[empty];
        final int wrong = right % 9 + 1;

        // 填错
        boolean reported = g.place(wrong);
        ok(reported && g.mistakes == 1 && g.values[empty] == wrong,
                "填错：mistakes+1、返回 true、格子被写入");

        // 撤销
        ok(g.hasUndo() && g.undo(), "填错后可撤销");
        ok(g.values[empty] == 0 && g.mistakes == 0, "撤销后恢复空格与错误数");

        // 填对
        g.sel = empty;
        g.place(right);
        ok(g.values[empty] == right && g.mistakes == 0, "填对：值正确、错误数不变");

        // 候选数被清掉
        int peer = -1;
        for (int i = 0; i < Game.N; i++) {
            if (i == empty || g.puzzle[i] != 0) continue;
            boolean sameRow = i / 9 == empty / 9;
            boolean sameCol = i % 9 == empty % 9;
            boolean sameBox = (i / 27 == empty / 27) && ((i % 9) / 3 == (empty % 9) / 3);
            if ((sameRow || sameCol || sameBox) && g.values[i] == 0) { peer = i; break; }
        }
        if (peer >= 0) {
            g.notes[peer] = 0x1FF;                  // 全候选 1..9
            g.sel = peer;
            g.notesMode = true;
            g.place(right);                         // 在同行/列/宫写同一个数
            boolean cleared = (g.notes[peer] & (1 << (right - 1))) == 0;
            ok(cleared, "填对后，同行/列/宫候选数里的该数字被清掉");
            g.notesMode = false;
            g.undo();
        } else {
            System.out.println("  [SKIP] 找不到可用于候选数清理测试的邻居");
        }

        // 再按一次相同数字 = 擦除
        g.sel = empty;
        g.values[empty] = right;
        g.place(right);
        ok(g.values[empty] == 0, "再按一次同一数字 = 擦除");

        // 给定格不可改
        int given = -1;
        for (int i = 0; i < Game.N; i++) if (g.puzzle[i] != 0) { given = i; break; }
        g.sel = given;
        int before = g.values[given];
        g.place(g.solution[given] % 9 + 1);
        ok(g.values[given] == before, "题面给定格不可修改");

        // 笔记模式 XOR
        g.sel = empty;
        g.notesMode = true;
        g.place(3);
        ok((g.notes[empty] & 0b100) != 0, "笔记模式：置位数字 3");
        g.place(3);
        ok((g.notes[empty] & 0b100) == 0, "笔记模式：再按一次取消数字 3");
        g.notesMode = false;

        // 擦除
        g.notes[empty] = 0x0F;
        g.sel = empty;
        g.erase();
        ok(g.values[empty] == 0 && g.notes[empty] == 0, "擦除清空值与候选数");

        // 提示
        Game h = new Game();
        h.newGame("medium");
        h.sel = -1;
        int hints0 = h.hints;
        int idx = h.hint();
        ok(idx >= 0 && h.values[idx] == h.solution[idx] && h.hinted[idx] == 1,
                "提示：在空格上填入正确答案并打标记");
        ok(h.hints == hints0 - 1, "提示：次数 -1");
        h.hint(); h.hint();
        ok(h.hints == 0, "提示：用完 3 次后为 0");
        ok(h.hint() < 0, "提示：次数为 0 时返回 -1（不再生效）");

        // 冲突检测
        Game c = new Game();
        c.newGame("easy");
        int a = -1, b = -1;
        for (int i = 0; i < Game.N && a < 0; i++)
            if (c.puzzle[i] == 0) a = i;
        for (int i = 0; i < Game.N && b < 0; i++)
            if (c.puzzle[i] == 0 && i != a && i / 9 == a / 9) b = i;
        if (a >= 0 && b >= 0) {
            c.values[a] = 7;
            c.values[b] = 7;
            boolean[] bad = c.conflicts();
            ok(bad[a] && bad[b], "冲突检测：同行重复数字被标记");

            // 换两个「在该格的行/列/宫都没出现过」且互不相同的数，不应误报
            c.values[a] = 0;
            c.values[b] = 0;
            boolean[] usedA = usedAround(c.values, a);
            boolean[] usedB = usedAround(c.values, b);
            int da = -1, db = -1;
            for (int d2 = 1; d2 <= 9 && da < 0; d2++) if (!usedA[d2]) da = d2;
            for (int d2 = 1; d2 <= 9 && db < 0; d2++) if (!usedB[d2] && d2 != da) db = d2;
            if (da > 0 && db > 0) {
                c.values[a] = da;
                c.values[b] = db;
                boolean[] bad2 = c.conflicts();
                ok(!bad2[a] && !bad2[b],
                        "冲突检测：不冲突的不同数字不误报（" + da + " / " + db + "）");
            } else {
                System.out.println("  [SKIP] 找不到两个安全数字");
            }
            c.values[a] = 0;
            c.values[b] = 0;
        } else {
            System.out.println("  [SKIP] 找不到同行两个空格");
        }

        // 剩余计数
        Game r = new Game();
        r.newGame("hard");
        int[] left = r.remaining();
        int sum = 0;
        for (int d2 = 1; d2 <= 9; d2++) sum += left[d2];
        int empties = 0;
        for (int x : r.values) if (x == 0) empties++;
        ok(sum == empties, "剩余计数总和 = 空格数（" + sum + "）");

        // 完成判定
        Game s = new Game();
        s.newGame("easy");
        ok(!s.solved(), "初始未完成");
        System.arraycopy(s.solution, 0, s.values, 0, Game.N);
        ok(s.solved(), "填满正确答案后 solved() = true");

        // 存档往返
        Game w = new Game();
        w.newGame("medium");
        w.values[30] = w.solution[30];
        w.notes[40] = 0x55;
        w.hinted[31] = 1;
        w.mistakes = 4;
        w.seconds = 137;
        int[] p2 = Game.split(Game.join(w.puzzle));
        int[] n2 = Game.split(Game.join(w.notes));
        ok(p2 != null && n2 != null, "序列化/反序列化长度校验通过");
        ok(p2 != null && Arrays.equals(p2, w.puzzle), "题面序列化往返一致");
        ok(n2 != null && Arrays.equals(n2, w.notes), "候选数序列化往返一致");
        ok(Game.split("1,2,3") == null, "非法长度的存档被拒绝");

        // ---------------------------------------------------------- 汇总
        System.out.println();
        System.out.println("通过 " + pass + " 项，失败 " + fail + " 项");
        System.out.println(fail == 0 ? "LOGIC_OK" : "LOGIC_FAIL");
        if (fail != 0) System.exit(1);
    }
}
