package com.buddy.sudoku;

/**
 * 配色令牌 —— 与桌面版 qt/palette.h 完全一致，取值逐个对照过来的。
 *
 * area / same / selected / conflict 是「accent 混入 bg 的百分比」（0..1），
 * 对应 Qt 里的 mixc(accent, bg, pct)。
 */
final class Palette {

    int bg, surface, text, hint, accent, onAccent, entry, mistake, good;
    float area, same, selected, conflict;

    Palette(int bg, int surface, int text, int hint, int accent, int onAccent,
            int entry, int mistake, int good,
            int area, int same, int selected, int conflict) {
        this.bg = bg;
        this.surface = surface;
        this.text = text;
        this.hint = hint;
        this.accent = accent;
        this.onAccent = onAccent;
        this.entry = entry;
        this.mistake = mistake;
        this.good = good;
        this.area = area / 100f;
        this.same = same / 100f;
        this.selected = selected / 100f;
        this.conflict = conflict / 100f;
    }

    static final class Theme {
        final String id, name;
        final Palette dark, light;

        Theme(String id, String name, Palette dark, Palette light) {
            this.id = id;
            this.name = name;
            this.dark = dark;
            this.light = light;
        }
    }

    static final Theme[] THEMES = {
        new Theme("classic", "经典",
            new Palette(0xFF161A29, 0xFF21263A, 0xFFEEF0F8, 0xFF8A90AA, 0xFF5B67E8, 0xFFFFFFFF,
                        0xFFA6AEFF, 0xFFFF6B72, 0xFF2E9E5B, 12, 26, 40, 20),
            new Palette(0xFFFFFFFF, 0xFFF1F2F7, 0xFF181A2B, 0xFF767B92, 0xFF4A56D8, 0xFFFFFFFF,
                        0xFF4A56D8, 0xFFC42B2B, 0xFF2E9E5B, 10, 22, 36, 20)),

        new Theme("paper", "纸质",
            new Palette(0xFF23201B, 0xFF2F2B25, 0xFFEFE8DA, 0xFFA1978A, 0xFF86AEDB, 0xFF1B1914,
                        0xFFA9C8EC, 0xFFFF8577, 0xFF58C48F, 11, 24, 36, 26),
            new Palette(0xFFF8F3E8, 0xFFEDE6D6, 0xFF2A2620, 0xFF73695B, 0xFF2F5D8A, 0xFFFFFFFF,
                        0xFF2F5D8A, 0xFFB0281F, 0xFF1F7A52, 9, 22, 34, 28)),

        new Theme("night", "夜色",
            new Palette(0xFF13112A, 0xFF1F1C3A, 0xFFECE9FF, 0xFF918BB8, 0xFF8A78FF, 0xFF110E24,
                        0xFFB4C4FF, 0xFFFF7290, 0xFF5FD1A7, 12, 28, 42, 28),
            new Palette(0xFFF4F1FB, 0xFFE8E3F5, 0xFF221D39, 0xFF6B6489, 0xFF6247D6, 0xFFFFFFFF,
                        0xFF5238C4, 0xFFB8233F, 0xFF1F7F66, 9, 22, 34, 26)),

        new Theme("garden", "花园",
            new Palette(0xFF121B14, 0xFF1C271E, 0xFFE5EFE2, 0xFF8B9B87, 0xFF5AB8C6, 0xFF0C140E,
                        0xFF8FD3DE, 0xFFFF8070, 0xFFD9B45F, 11, 26, 40, 30),
            new Palette(0xFFF4F6EE, 0xFFE5EAD9, 0xFF1D2A1E, 0xFF63715D, 0xFF1F6873, 0xFFFFFFFF,
                        0xFF1F6873, 0xFFB3261E, 0xFF7A5A12, 9, 20, 32, 32)),

        new Theme("contrast", "高对比",
            new Palette(0xFF000000, 0xFF1A1A1A, 0xFFFFFFFF, 0xFFA6A6A6, 0xFFFFD400, 0xFF000000,
                        0xFF5CD2FF, 0xFFFF5C7A, 0xFF4FE0A0, 10, 22, 34, 34),
            new Palette(0xFFFFFFFF, 0xFFEDEDED, 0xFF000000, 0xFF555555, 0xFF0035C8, 0xFFFFFFFF,
                        0xFF0035C8, 0xFFA33A00, 0xFF007A33, 10, 22, 36, 26)),

        new Theme("arcade", "街机",
            new Palette(0xFF0E0B16, 0xFF1B1628, 0xFFF3EDFF, 0xFF938AAD, 0xFFFF4FD8, 0xFF16000F,
                        0xFF46EAFF, 0xFFFFB02E, 0xFF5CFF9D, 12, 26, 38, 38),
            new Palette(0xFFFBF7FF, 0xFFEFE7FB, 0xFF1E1430, 0xFF706287, 0xFF7A3FF2, 0xFFFFFFFF,
                        0xFF0A6FB0, 0xFFB8213F, 0xFF0F8F5E, 9, 22, 34, 26)),
    };

    static Theme theme(String id) {
        for (Theme t : THEMES) if (t.id.equals(id)) return t;
        return THEMES[0];
    }

    /** 当前生效的调色板（由 Prefs.applyTheme() 替换） */
    static Palette cur = THEMES[0].dark;

    // ---------------------------------------------------------------- 颜色工具

    /** Qt 的 mixc(a, b, t)：t=1 全取 a，t=0 全取 b */
    static int mix(int a, int b, float t) {
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
        int br = (b >> 16) & 0xFF, bgc = (b >> 8) & 0xFF, bb = b & 0xFF;
        int r = Math.round(ar * t + br * (1 - t));
        int g = Math.round(ag * t + bgc * (1 - t));
        int bl = Math.round(ab * t + bb * (1 - t));
        return 0xFF000000 | (r << 16) | (g << 8) | bl;
    }

    /** 给颜色套一个不透明度 0..1 */
    static int alpha(int c, float a) {
        int al = Math.round(255 * Math.max(0f, Math.min(1f, a)));
        return (c & 0x00FFFFFF) | (al << 24);
    }
}
