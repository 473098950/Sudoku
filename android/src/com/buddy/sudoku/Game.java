package com.buddy.sudoku;

import java.util.ArrayDeque;
import java.util.Random;

/**
 * 数独核心逻辑 —— 由桌面版 qt/game.h 逐行移植。
 *
 * 出题保证唯一解：随机生成完整解 → 随机顺序挖洞 → 每挖一格用
 * countSolutions(probe, 2) 验证，只有仍然唯一才保留，否则还原。
 */
final class Game {

    static final int N = 81;

    final int[] puzzle = new int[N];
    final int[] solution = new int[N];
    final int[] values = new int[N];
    final int[] notes = new int[N];
    final int[] hinted = new int[N];

    int sel = -1;
    boolean notesMode = false;
    int mistakes = 0, seconds = 0, hints = 3;
    String diff = "medium";
    boolean done = false, running = true;
    /** 手动/自动暂停：为 true 时计时器不走，且不接受任何落子输入 */
    boolean paused = false;

    final Random rnd = new Random();

    /** 每格 20 个「同行 / 同列 / 同宫」的邻居（去重，不含自己） */
    private static final int[][] PEERS = buildPeers();

    private static final class Snap {
        final int[] values, notes, hinted;
        final int mistakes;

        Snap(int[] v, int[] n, int[] h, int m) {
            values = v;
            notes = n;
            hinted = h;
            mistakes = m;
        }
    }

    private final ArrayDeque<Snap> history = new ArrayDeque<>();

    // ------------------------------------------------------------ 邻居表

    private static int[][] buildPeers() {
        int[][] out = new int[N][20];
        for (int i = 0; i < N; i++) {
            final int r = i / 9, c = i % 9;
            final int br = r - r % 3, bc = c - c % 3;
            boolean[] seen = new boolean[N];
            int n = 0;
            for (int k = 0; k < 9; k++) {
                int a = r * 9 + k, b = k * 9 + c;
                if (a != i && !seen[a]) { seen[a] = true; out[i][n++] = a; }
                if (b != i && !seen[b]) { seen[b] = true; out[i][n++] = b; }
            }
            for (int x = 0; x < 3; x++)
                for (int y = 0; y < 3; y++) {
                    int p = (br + x) * 9 + bc + y;
                    if (p != i && !seen[p]) { seen[p] = true; out[i][n++] = p; }
                }
        }
        return out;
    }

    // ------------------------------------------------------------ 求解器

    /** 返回第 i 格的「已用数字位掩码」 */
    private static int maskAt(int[] g, int i) {
        final int r = i / 9, c = i % 9;
        final int br = r - r % 3, bc = c - c % 3;
        int m = 0;
        for (int k = 0; k < 9; k++) {
            m |= 1 << g[r * 9 + k];
            m |= 1 << g[k * 9 + c];
        }
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++)
                m |= 1 << g[(br + a) * 9 + bc + b];
        return m & ~1;                     // 去掉 0 号位（空格）
    }

    private int[] genFull() {
        int[] g = new int[N];
        boolean ok = fill(g, 0);
        if (!ok) throw new IllegalStateException("生成完整解失败");
        return g;
    }

    private boolean fill(int[] g, int i) {
        if (i == N) return true;
        final int used = maskAt(g, i);
        int[] ds = new int[9];
        int n = 0;
        for (int d = 1; d <= 9; d++) if ((used & (1 << d)) == 0) ds[n++] = d;
        for (int k = n - 1; k > 0; k--) {
            int j = rnd.nextInt(k + 1);
            int t = ds[k]; ds[k] = ds[j]; ds[j] = t;
        }
        for (int k = 0; k < n; k++) {
            g[i] = ds[k];
            if (fill(g, i + 1)) return true;
            g[i] = 0;
        }
        return false;
    }

    /** MRV + 限界计数：数到 limit 就提前返回 */
    private static int countSolutions(int[] g, int limit) {
        int best = -1, bestMask = 0, bestCnt = 10;
        for (int i = 0; i < N; i++) {
            if (g[i] != 0) continue;
            final int used = maskAt(g, i);
            int cnt = 0;
            for (int d = 1; d <= 9; d++) if ((used & (1 << d)) == 0) cnt++;
            if (cnt == 0) return 0;        // 死格，无解
            if (cnt < bestCnt) {
                bestCnt = cnt;
                best = i;
                bestMask = used;
                if (cnt == 1) break;
            }
        }
        if (best < 0) return 1;            // 全填满，找到一个解
        int total = 0;
        for (int d = 1; d <= 9; d++) {
            if ((bestMask & (1 << d)) != 0) continue;
            g[best] = d;
            total += countSolutions(g, limit - total);
            g[best] = 0;
            if (total >= limit) return total;
        }
        return total;
    }

    // ------------------------------------------------------------ 出题

    private static int givensFor(String diff) {
        switch (diff) {
            case "easy":   return 44;
            case "hard":   return 29;
            case "expert": return 25;
            default:       return 34;
        }
    }

    private void makePuzzle(String d) {
        int[] sol = genFull();
        int[] puz = sol.clone();
        final int target = N - givensFor(d);

        int[] order = new int[N];
        for (int i = 0; i < N; i++) order[i] = i;
        for (int k = N - 1; k > 0; k--) {
            int j = rnd.nextInt(k + 1);
            int t = order[k]; order[k] = order[j]; order[j] = t;
        }

        final long deadline = System.nanoTime() + 1_600_000_000L;   // 1.6s 保护
        int removed = 0;
        for (int idx : order) {
            if (removed >= target || System.nanoTime() > deadline) break;
            final int bak = puz[idx];
            if (bak == 0) continue;
            puz[idx] = 0;
            if (countSolutions(puz.clone(), 2) != 1) puz[idx] = bak;
            else removed++;
        }

        System.arraycopy(puz, 0, puzzle, 0, N);
        System.arraycopy(sol, 0, solution, 0, N);
    }

    // ------------------------------------------------------------ 生命周期

    void newGame(String d) {
        makePuzzle(d);
        System.arraycopy(puzzle, 0, values, 0, N);
        java.util.Arrays.fill(notes, 0);
        java.util.Arrays.fill(hinted, 0);
        sel = -1;
        mistakes = 0;
        seconds = 0;
        hints = 3;
        diff = d;
        done = false;
        running = true;
        paused = false;
        history.clear();
    }

    void clearAll() {
        java.util.Arrays.fill(puzzle, 0);
        java.util.Arrays.fill(solution, 0);
        java.util.Arrays.fill(values, 0);
        java.util.Arrays.fill(notes, 0);
        java.util.Arrays.fill(hinted, 0);
        history.clear();
    }

    private void snap() {
        history.addLast(new Snap(values.clone(), notes.clone(), hinted.clone(), mistakes));
        while (history.size() > 300) history.removeFirst();
    }

    boolean undo() {
        if (history.isEmpty()) return false;
        Snap s = history.removeLast();
        System.arraycopy(s.values, 0, values, 0, N);
        System.arraycopy(s.notes, 0, notes, 0, N);
        System.arraycopy(s.hinted, 0, hinted, 0, N);
        mistakes = s.mistakes;
        return true;
    }

    boolean hasUndo() {
        return !history.isEmpty();
    }

    // ------------------------------------------------------------ 操作

    /** 返回 true 表示这一步填错了（用于震动/提示） */
    boolean place(int d) {
        final int i = sel;
        if (i < 0 || done || puzzle[i] != 0) return false;

        if (notesMode) {
            if (values[i] != 0) return false;
            snap();
            notes[i] ^= (1 << (d - 1));
            return false;
        }
        if (values[i] == d) {              // 再按一次同一个数字 = 擦掉
            snap();
            values[i] = 0;
            hinted[i] = 0;
            return false;
        }
        snap();
        values[i] = d;
        notes[i] = 0;
        if (d == solution[i]) {
            hinted[i] = 0;
            final int keep = ~(1 << (d - 1));
            for (int p : PEERS[i]) notes[p] &= keep;   // 清掉同行的候选数
            return false;
        }
        mistakes++;
        return true;
    }

    void erase() {
        final int i = sel;
        if (i < 0 || done || puzzle[i] != 0) return;
        if (values[i] == 0 && notes[i] == 0) return;
        snap();
        values[i] = 0;
        notes[i] = 0;
        hinted[i] = 0;
    }

    /** 返回被揭示的格号，-1 表示没有可提示的 */
    int hint() {
        if (done || hints <= 0) return -1;
        int i = sel;
        if (i < 0 || puzzle[i] != 0 || values[i] == solution[i]) {
            int cnt = 0;
            for (int k = 0; k < N; k++) if (puzzle[k] == 0 && values[k] != solution[k]) cnt++;
            if (cnt == 0) return -1;
            int pick = rnd.nextInt(cnt);
            i = -1;
            for (int k = 0; k < N; k++)
                if (puzzle[k] == 0 && values[k] != solution[k] && pick-- == 0) { i = k; break; }
            if (i < 0) return -1;
            sel = i;
        }
        snap();
        hints--;
        values[i] = solution[i];
        hinted[i] = 1;
        notes[i] = 0;
        final int keep = ~(1 << (solution[i] - 1));
        for (int p : PEERS[i]) notes[p] &= keep;
        return i;
    }

    boolean solved() {
        for (int i = 0; i < N; i++) if (values[i] != solution[i]) return false;
        return true;
    }

    /** 冲突格集合（同行/列/宫出现重复数字） */
    boolean[] conflicts() {
        boolean[] bad = new boolean[N];
        for (int i = 0; i < N; i++) {
            final int v = values[i];
            if (v == 0) continue;
            final int r = i / 9, c = i % 9;
            for (int k = 0; k < 9; k++) {
                int a = r * 9 + k, b = k * 9 + c;
                if (a != i && values[a] == v) { bad[i] = true; bad[a] = true; }
                if (b != i && values[b] == v) { bad[i] = true; bad[b] = true; }
            }
            final int br = r - r % 3, bc = c - c % 3;
            for (int x = 0; x < 3; x++)
                for (int y = 0; y < 3; y++) {
                    int p = (br + x) * 9 + bc + y;
                    if (p != i && values[p] == v) { bad[i] = true; bad[p] = true; }
                }
        }
        return bad;
    }

    /** 每个数字还剩几个没放：left[1..9] */
    int[] remaining() {
        int[] used = new int[10];
        for (int v : values) if (v != 0) used[v]++;
        int[] left = new int[10];
        for (int d = 1; d <= 9; d++) left[d] = 9 - used[d];
        return left;
    }

    boolean started() {
        for (int i = 0; i < N; i++) if (values[i] != puzzle[i]) return true;
        return false;
    }

    // ------------------------------------------------------------ 存档序列化

    static String join(int[] v) {
        StringBuilder sb = new StringBuilder(v.length * 2);
        for (int i = 0; i < v.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(v[i]);
        }
        return sb.toString();
    }

    static int[] split(String s) {
        if (s == null || s.isEmpty()) return null;
        String[] p = s.split(",");
        if (p.length != N) return null;
        int[] out = new int[N];
        try {
            for (int i = 0; i < N; i++) out[i] = Integer.parseInt(p[i].trim());
        } catch (NumberFormatException e) {
            return null;
        }
        return out;
    }
}
