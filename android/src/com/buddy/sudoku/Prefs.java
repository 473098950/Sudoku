package com.buddy.sudoku;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * 存档 / 主题偏好。键名与桌面版 settings.ini 一一对应，方便两边概念对齐。
 */
final class Prefs {

    private static SharedPreferences sp;

    private Prefs() {
    }

    static void init(Context c) {
        if (sp == null) sp = c.getApplicationContext()
                .getSharedPreferences("sudoku", Context.MODE_PRIVATE);
    }

    static String theme() {
        return sp.getString("theme", "classic");
    }

    static void setTheme(String v) {
        sp.edit().putString("theme", v).apply();
    }

    static String scheme() {
        return sp.getString("scheme", "dark");
    }

    static void setScheme(String v) {
        sp.edit().putString("scheme", v).apply();
    }

    /** 把 theme + scheme 解析成当前生效的调色板 */
    static void applyTheme() {
        Palette.Theme t = Palette.theme(theme());
        Palette.cur = "light".equals(scheme()) ? t.light : t.dark;
    }

    static boolean isLightScheme() {
        return "light".equals(scheme());
    }

    // ------------------------------------------------------------ 棋局

    static void saveGame(Game g) {
        sp.edit()
                .putString("board/puzzle", Game.join(g.puzzle))
                .putString("board/solution", Game.join(g.solution))
                .putString("board/values", Game.join(g.values))
                .putString("board/notes", Game.join(g.notes))
                .putString("board/hinted", Game.join(g.hinted))
                .putString("board/diff", g.diff)
                .putInt("board/mistakes", g.mistakes)
                .putInt("board/seconds", g.seconds)
                .putInt("board/hints", g.hints)
                .putBoolean("board/done", g.done)
                .putBoolean("board/paused", g.paused)
                .apply();
    }

    /** @return true 表示成功恢复了上次的棋局 */
    static boolean loadGame(Game g) {
        int[] puz = Game.split(sp.getString("board/puzzle", null));
        int[] sol = Game.split(sp.getString("board/solution", null));
        if (puz == null || sol == null) return false;

        System.arraycopy(puz, 0, g.puzzle, 0, Game.N);
        System.arraycopy(sol, 0, g.solution, 0, Game.N);

        int[] vals = Game.split(sp.getString("board/values", null));
        System.arraycopy(vals != null ? vals : puz, 0, g.values, 0, Game.N);

        int[] notes = Game.split(sp.getString("board/notes", null));
        if (notes != null) System.arraycopy(notes, 0, g.notes, 0, Game.N);

        int[] hinted = Game.split(sp.getString("board/hinted", null));
        if (hinted != null) System.arraycopy(hinted, 0, g.hinted, 0, Game.N);

        g.diff = sp.getString("board/diff", "medium");
        g.mistakes = sp.getInt("board/mistakes", 0);
        g.seconds = sp.getInt("board/seconds", 0);
        g.hints = sp.getInt("board/hints", 3);
        g.done = sp.getBoolean("board/done", false);
        // 冷启动不继承暂停态：暂停表示"人暂时离开了这个界面"，
        // 而重新打开 App 说明人已经在场，一上来就停在暂停层会像是卡住了。
        // （board/paused 仍然写盘，只用于异常退出后排查，不再回读。）
        g.paused = false;
        g.sel = -1;
        g.running = !g.done;
        return true;
    }
}
