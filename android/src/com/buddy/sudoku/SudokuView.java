package com.buddy.sudoku;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * 全自绘界面 —— 对应桌面版 qt/widgets.cpp 里 Board / Pill / IconBtn / KeyBtn /
 * DiffBtn / cardDialog 的 paintEvent，合并在一个 View 里画。
 *
 * 尺寸系统的核心是 u：
 *     u = 棋盘边长(px) / 400
 * 桌面版的设计稿是「棋盘 400、按 k 等比缩放」，这里把 k 换成 u，
 * 于是所有 Qt 里的长度 L 都变成 L*u，所有字号 pt 都变成 pt*1.3333*u
 * （Qt 点值按 96dpi 折算成像素再乘缩放比）。这样无论 1080p 还是 2K 屏，
 * 棋盘与文字的相对比例都与桌面版完全一致。
 */
public class SudokuView extends View {

    public interface Callback {
        void onStateChanged();     // 棋盘内容变了，需要存档
        void onChangeSkin();       // 主题/明暗变了，需要同步系统栏图标颜色
    }

    // ------------------------------------------------------------------ 常量

    private static final int KIND_BOARD = 1;
    private static final int KIND_KEY   = 2;
    private static final int KIND_ICON  = 3;
    private static final int KIND_DIFF  = 4;
    private static final int KIND_ACT   = 5;   // 菜单项 / 对话框按钮
    private static final int KIND_TIME  = 6;   // 左上角计时器胶囊（点它开始暂停）
    private static final int KIND_ACTBAR = 7;  // 数字键下面那排（回退 / 笔记）

    private static final int IC_UNDO = 0, IC_NOTES = 1, IC_ERASE = 2,
                             IC_HINT = 3, IC_NEW = 4, IC_THEME = 5;

    private static final String[] ICON_NAMES = {"undo", "pencil", "eraser", "bulb", "plus", "palette"};

    private static final int OV_NONE = 0, OV_DIFF = 1, OV_THEME = 2, OV_CARD = 3, OV_PAUSE = 4;

    private static final String[] DIFFS = {"easy", "medium", "hard", "expert"};
    private static final String[] DIFF_CN = {"简单", "中等", "困难", "专家"};

    private static final int ACT_CLOSE   = 100;
    private static final int ACT_CARD_1  = 101;
    private static final int ACT_CARD_2  = 102;
    private static final int ACT_THEME   = 200;   // 200..205 主题
    private static final int ACT_SCHEME  = 300;   // 300 深色 / 301 浅色
    private static final int ACT_DIFF    = 400;   // 400..403 难度
    private static final int ACT_RESUME  = 500;   // 暂停浮层里的三角按钮
    private static final int ACT_NOOP    = 501;   // 暂停浮层的遮罩（吃掉点击，不关界面）

    // ------------------------------------------------------------------ 静态资源

    private static final Typeface TF_REG = Typeface.create("sans-serif", Typeface.NORMAL);
    private static final Typeface TF_MED = Typeface.create("sans-serif-medium", Typeface.NORMAL);

    // ------------------------------------------------------------------ 字段

    private Game g;
    private Callback cb;

    private float f = 3f;      // px per dp
    private float u = 2.8f;    // px per Qt 设计单位

    private int insetTop, insetBottom;

    private float boardSize;
    private final RectF rTb1 = new RectF();
    private final RectF rTb2 = new RectF();
    private final RectF rBoard = new RectF();
    private final RectF rKeys = new RectF();
    private final RectF rActs = new RectF();     // 数字键下面那排：回退 / 笔记

    private float pillTimeW;      // 上一帧测得的胶囊宽度，用于布局命中区
    private float pillMissW;
    private float diffW;

    // 交互状态
    private static final class Hit {
        final RectF r = new RectF();
        int kind, arg;
        boolean pressable;
    }

    private final List<Hit> hits = new ArrayList<>();
    private int pressKind = -1, pressArg = -1;
    private long noSelFlashUntil = 0;

    private int overlay = OV_NONE;
    private final RectF rOvPanel = new RectF();

    // 卡片对话框状态
    private String cardTitle = "", cardSub = "";
    private String cardK1 = null, cardV1 = null, cardK2 = null, cardV2 = null;
    private String cardPrimary = "", cardSecondary = "";
    private Runnable cardAction;

    // 画笔
    private final Paint pFill   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pText   = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    private final Paint pIcon   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path  path    = new Path();
    private final RectF tmp     = new RectF();
    private final Paint.FontMetrics fm = new Paint.FontMetrics();

    // ------------------------------------------------------------------ 构造

    public SudokuView(Context ctx, Game game) {
        super(ctx);
        g = game;
        f = getResources().getDisplayMetrics().density;
        pIcon.setStyle(Paint.Style.STROKE);
        pIcon.setStrokeCap(Paint.Cap.ROUND);
        pIcon.setStrokeJoin(Paint.Join.ROUND);
        setFocusable(true);
    }

    public void setCallback(Callback c) {
        cb = c;
    }

    public void setSystemInsets(int top, int bottom) {
        if (top == insetTop && bottom == insetBottom) return;
        insetTop = top;
        insetBottom = bottom;
        requestLayout();
        invalidate();
    }

    public void setGame(Game game) {
        g = game;
        invalidate();
    }

    /** 主题变了：外部已改 Palette.cur，这里只需要重绘 */
    public void onSkinChanged() {
        invalidate();
    }

    public void notifyChanged() {
        invalidate();
        if (cb != null) cb.onStateChanged();
    }

    // ------------------------------------------------------------------ 布局

    @Override
    protected void onMeasure(int wSpec, int hSpec) {
        super.onMeasure(wSpec, hSpec);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        computeLayout(w, h);
    }

    /**
     * 纵向分配策略：
     *
     *   工具栏两行   —— 贴顶，紧跟状态栏
     *   棋盘 + 键盘 + 回退/笔记条 —— 当成一整块，在"工具栏下方 → 屏幕底部"之间居中
     *
     * 键盘贴在棋盘正下方（间隔 20dp），不再贴屏幕最底：19.5:9 的长屏把富余高度
     * 全堆在棋盘和键盘之间时，键盘会掉到 90% 屏高以下，拇指要往下够，很难点。
     * 现在键盘落在 70~84% 屏高这一段，拇指自然落点。
     */
    private void computeLayout(int w, int h) {
        final float pad = 14 * f;
        final float availTop = insetTop + 14 * f;
        final float availBottom = h - insetBottom - 16 * f;

        final float tb1H = 34 * f;
        final float gap1 = 12 * f;
        final float tb2H = 44 * f;
        final float keyH = 46 * f;
        final float gapKey = 20 * f;      // 棋盘与键盘之间的间距
        final float actH = 56 * f;        // 回退 / 笔记条高度（比数字键 46dp 略高，宽扁按钮才不至于显瘦）
        final float gapAct = 10 * f;      // 键盘与回退/笔记条之间的间距
        final float gapMin = 18 * f;      // 棋盘与工具栏之间的最小间距

        // 顶部两行工具栏：贴顶
        rTb1.set(pad, availTop, w - pad, availTop + tb1H);
        rTb2.set(pad, rTb1.bottom + gap1, w - pad, rTb1.bottom + gap1 + tb2H);

        final float bandTop = rTb2.bottom + gapMin;
        final float band = availBottom - bandTop;

        // 棋盘先按"宽度"和"扣掉键盘与按钮条后的高度"取小值
        final float belowBoard = gapKey + keyH + gapAct + actH;
        boardSize = Math.min(w - 2 * pad, band - belowBoard);
        boardSize = Math.max(boardSize, 120 * f);
        u = boardSize / 400f;

        final float groupH = boardSize + belowBoard;
        final float air = Math.max(0f, band - groupH);
        float top = bandTop + air * 0.5f;
        if (top + groupH > availBottom) top = Math.max(bandTop, availBottom - groupH);

        final float cx = w / 2f;
        rBoard.set(cx - boardSize / 2f, top,
                   cx + boardSize / 2f, top + boardSize);
        rKeys.set(rBoard.left, rBoard.bottom + gapKey,
                  rBoard.right, rBoard.bottom + gapKey + keyH);
        rActs.set(rBoard.left, rKeys.bottom + gapAct,
                  rBoard.right, rKeys.bottom + gapAct + actH);

        pillTimeW = 0;
        pillMissW = 0;
        diffW = 0;
    }

    // ------------------------------------------------------------------ 绘制入口

    @Override
    protected void onDraw(Canvas c) {
        super.onDraw(c);
        hits.clear();

        final Palette p = Palette.cur;
        c.drawColor(p.bg);

        drawToolbar1(c, p);
        drawToolbar2(c, p);
        drawBoard(c, p);
        drawKeypad(c, p);
        drawActionBar(c, p);

        if (overlay != OV_NONE) drawOverlay(c, p);
    }

    // ================================================================== 第一行：胶囊 + 难度

    /**
     * 画一个状态胶囊，返回其右边界 x。
     *
     * @param bad     错误数为正时整体转红
     * @param hot     暂停态：图标变三角、文字变 accent，提示"点我可以继续"
     * @param hitKind 非 0 时登记点击区（计时器胶囊用）
     */
    private float drawPill(Canvas c, Palette p, String icon, String label,
                           boolean bad, boolean hot, float x, float cy, int hitKind) {
        pText.setTypeface(TF_MED);
        pText.setTextSize(13 * f);
        pText.setTextAlign(Paint.Align.LEFT);
        final float tw = pText.measureText(label);

        final float h = 34 * f;
        final float iconBox = 15 * f;
        final float iconX = 11 * f;
        final float textX = iconX + iconBox + 6 * f;
        final float w = textX + tw + 13 * f;

        tmp.set(x, cy - h / 2f, x + w, cy + h / 2f);
        pFill.setColor(p.surface);
        c.drawRoundRect(tmp, 12 * f, 12 * f, pFill);
        if (hitKind != 0) addHit(tmp, hitKind, 0, true);

        final int ic = bad ? p.mistake : (hot ? p.accent : p.hint);
        drawGlyph(c, icon, ic, iconBox, x + iconX + iconBox / 2f, cy);

        pText.setColor(bad ? p.mistake : (hot ? p.accent : p.text));
        drawTextV(c, label, x + textX, cy, pText);

        return x + w;
    }

    private void drawToolbar1(Canvas c, Palette p) {
        final float cy = rTb1.centerY();
        final float gap = 8 * f;

        float x = rTb1.left;
        // 暂停时换成三角图标 + accent 文字，既是状态提示也是"点这里继续"的入口
        x = drawPill(c, p, g.paused ? "play" : "clock", timeLabel(),
                     false, g.paused, x, cy, KIND_TIME) + gap;
        pillTimeW = x - gap - rTb1.left;
        final float missLeft = x;
        x = drawPill(c, p, "cross", String.valueOf(g.mistakes),
                     g.mistakes > 0, false, x, cy, 0);
        pillMissW = x - missLeft;

        // 难度：右对齐
        final float dw = 92 * f, dh = 34 * f;
        diffW = dw;
        tmp.set(rTb1.right - dw, cy - dh / 2f, rTb1.right, cy + dh / 2f);
        pFill.setColor(p.surface);
        c.drawRoundRect(tmp, 12 * f, 12 * f, pFill);

        pText.setTypeface(TF_MED);
        pText.setTextSize(13 * f);
        pText.setTextAlign(Paint.Align.LEFT);
        pText.setColor(p.text);
        drawTextV(c, diffLabel(g.diff), tmp.left + 14 * f, cy, pText);
        drawGlyph(c, "chevron", p.hint, 13 * f, tmp.right - 18 * f, cy);

        Hit hit = addHit(tmp, KIND_DIFF, 0, true);
        hit.hashCode();
    }

    private static String diffLabel(String d) {
        for (int i = 0; i < DIFFS.length; i++) if (DIFFS[i].equals(d)) return DIFF_CN[i];
        return "中等";
    }

    private String timeLabel() {
        return String.format(Locale.US, "%02d:%02d", g.seconds / 60, g.seconds % 60);
    }

    // ================================================================== 第二行：图标按钮

    private void drawToolbar2(Canvas c, Palette p) {
        final float size = 44 * f;
        final float n = ICON_NAMES.length;
        final float gap = Math.min(14 * f, (rTb2.width() - n * size) / (n - 1));
        float x = rTb2.right - n * size - (n - 1) * gap;
        final float cy = rTb2.centerY();

        for (int i = 0; i < n; i++) {
            final boolean on = (i == IC_NOTES && g.notesMode);
            boolean enabled = true;
            if (i == IC_UNDO) enabled = g.hasUndo();
            if (i == IC_HINT) enabled = !g.done && g.hints > 0;

            tmp.set(x, cy - size / 2f, x + size, cy + size / 2f);
            final boolean isPressed = pressKind == KIND_ICON && pressArg == i;

            if (on) {
                pFill.setColor(Palette.mix(p.accent, p.bg, p.same));
                c.drawRoundRect(tmp, 13 * f, 13 * f, pFill);
            } else if (isPressed && enabled) {
                pFill.setColor(p.surface);
                c.drawRoundRect(tmp, 13 * f, 13 * f, pFill);
            }

            int col = on ? p.entry : (isPressed ? p.text : p.hint);
            // 不可用态：Qt 是「颜色 30% 不透明」再叠「整体 45% 透明度」→ 等效 13.5%
            if (!enabled) col = Palette.alpha(p.hint, 0.135f);
            drawGlyph(c, ICON_NAMES[i], col, size * 0.61f, tmp.centerX(), cy);

            addHit(tmp, KIND_ICON, i, enabled);
            x += size + gap;
        }
    }

    // ================================================================== 棋盘

    private void drawBoard(Canvas c, Palette p) {
        final boolean[] bad = g.conflicts();
        final int si = g.sel;
        final int sv = si >= 0 ? g.values[si] : 0;
        final int sr = si >= 0 ? si / 9 : -1;
        final int sc = si >= 0 ? si % 9 : -1;
        final int sbr = sr >= 0 ? sr - sr % 3 : -9;
        final int sbc = sc >= 0 ? sc - sc % 3 : -9;

        final float border = Math.max(1f, 2.0f * u);
        final float radius = Math.max(2f, 8.0f * u);
        final float cw = (rBoard.width() - 2 * border) / 9f;
        final float ch = (rBoard.height() - 2 * border) / 9f;

        final int cArea = Palette.mix(p.accent, p.bg, p.area);
        final int cSame = Palette.mix(p.accent, p.bg, p.same);
        final int cSel  = Palette.mix(p.accent, p.bg, p.selected);
        final int cDup  = Palette.mix(p.mistake, p.bg, p.conflict);

        // 圆角裁剪
        final int save = c.save();
        tmp.set(rBoard);
        path.reset();
        path.addRoundRect(tmp, radius, radius, Path.Direction.CW);
        c.clipPath(path);

        c.drawRect(rBoard, solid(p.bg));

        final float digitSize = 20.2f * 1.3333f * u;
        final float noteSize  = 8.0f * 1.3333f * u;

        for (int i = 0; i < Game.N; i++) {
            final int r = i / 9, col = i % 9;
            final float x = rBoard.left + border + col * cw;
            final float y = rBoard.top + border + r * ch;
            tmp.set(x, y, x + cw, y + ch);

            final int v = g.values[i];
            final boolean isSel = (i == si);
            final boolean isPeer = si >= 0 &&
                    (r == sr || col == sc ||
                     (r >= sbr && r < sbr + 3 && col >= sbc && col < sbc + 3));

            boolean has = false;
            int bg = 0;
            if (sv != 0) {
                if (v == sv) { bg = cSame; has = true; }
                else if (v == 0 && isPeer) { bg = cArea; has = true; }
            } else if (isPeer && !isSel) {
                bg = cArea;
                has = true;
            }
            if (bad[i]) { bg = cDup; has = true; }
            if (isSel) { bg = cSel; has = true; }
            if (has) c.drawRect(tmp, solid(bg));

            if (v != 0) {
                int fg;
                Typeface tf;
                if (g.puzzle[i] != 0) { fg = p.text; tf = TF_MED; }
                else if (v == g.solution[i]) { fg = p.entry; tf = TF_REG; }
                else { fg = p.mistake; tf = TF_REG; }
                if (g.hinted[i] != 0) fg = p.entry;

                pText.setTypeface(tf);
                pText.setTextSize(digitSize);
                pText.setTextAlign(Paint.Align.CENTER);
                pText.setColor(fg);
                final boolean dim = sv != 0 && v != sv && !isSel;
                if (dim) pText.setAlpha(102);
                drawTextV(c, String.valueOf(v), tmp.centerX(), tmp.centerY(), pText);
                if (dim) pText.setAlpha(255);
            } else if (g.notes[i] != 0) {
                pText.setTypeface(TF_REG);
                pText.setTextSize(noteSize);
                pText.setTextAlign(Paint.Align.CENTER);
                pText.setColor(p.hint);
                for (int d = 1; d <= 9; d++) {
                    if ((g.notes[i] & (1 << (d - 1))) == 0) continue;
                    final float nx = x + ((d - 1) % 3) * cw / 3f;
                    final float ny = y + ((d - 1) / 3) * ch / 3f;
                    drawTextV(c, String.valueOf(d), nx + cw / 6f, ny + ch / 6f, pText);
                }
            }
        }

        // 网格线
        final float wThin = Math.max(0.7f * f, 1.0f * u);
        final float wRule = Math.max(1.5f * f, 2.0f * u);
        pStroke.setStyle(Paint.Style.STROKE);
        for (int i = 1; i < 9; i++) {
            final boolean rule = (i % 3 == 0);
            pStroke.setColor(Palette.alpha(p.text, rule ? 0.62f : 0.18f));
            pStroke.setStrokeWidth(rule ? wRule : wThin);
            final float gx = rBoard.left + border + i * cw;
            final float gy = rBoard.top + border + i * ch;
            c.drawLine(gx, rBoard.top + border, gx, rBoard.bottom - border, pStroke);
            c.drawLine(rBoard.left + border, gy, rBoard.right - border, gy, pStroke);
        }

        // 选中态描边
        if (si >= 0) {
            final int r = si / 9, col = si % 9;
            tmp.set(rBoard.left + border + col * cw, rBoard.top + border + r * ch,
                    rBoard.left + border + col * cw + cw, rBoard.top + border + r * ch + ch);
            final float inset = Math.max(0.4f * f, 0.8f * u);
            tmp.inset(inset, inset);
            pStroke.setColor(p.accent);
            pStroke.setStrokeWidth(Math.max(1.0f * f, 1.6f * u));
            final float rr = Math.max(0.8f * f, 2.0f * u);
            c.drawRoundRect(tmp, rr, rr, pStroke);
        }

        c.restoreToCount(save);

        // 外框（"先点格子"提示时闪 accent 色）
        tmp.set(rBoard);
        final boolean flash = System.currentTimeMillis() < noSelFlashUntil;
        pStroke.setColor(flash ? p.accent : Palette.alpha(p.text, 0.62f));
        pStroke.setStrokeWidth(flash ? Math.max(2.5f * f, border * 1.8f) : border);
        c.drawRoundRect(tmp, radius, radius, pStroke);

        // 命中区：81 个格子
        for (int i = 0; i < Game.N; i++) {
            final int r = i / 9, col = i % 9;
            final float x = rBoard.left + border + col * cw;
            final float y = rBoard.top + border + r * ch;
            Hit h = new Hit();
            h.r.set(x, y, x + cw, y + ch);
            h.kind = KIND_BOARD;
            h.arg = i;
            h.pressable = false;
            hits.add(h);
        }
    }

    // ================================================================== 数字键盘

    private void drawKeypad(Canvas c, Palette p) {
        final int[] left = g.remaining();
        final float gap = 4 * f;
        final float kw = (rKeys.width() - gap * 8) / 9f;
        final float kh = rKeys.height();

        // 数字尺寸同时受「键高」和「键宽」约束：只按高度算会让 19.5:9 屏上
        // 的窄键里数字横向顶满、角标压到数字上。
        final float digitSize = Math.min(kh * 0.58f, kw * 0.62f);
        final float badgeSize = Math.min(kh * 0.22f, kw * 0.28f);

        for (int d = 1; d <= 9; d++) {
            final float x = rKeys.left + (d - 1) * (kw + gap);
            tmp.set(x, rKeys.top, x + kw, rKeys.bottom);

            final boolean done = left[d] <= 0;
            final boolean isPressed = pressKind == KIND_KEY && pressArg == d;

            int bg = p.surface;
            if (!done) {
                if (isPressed) bg = Palette.mix(p.accent, p.bg, (p.selected - 0.02f));
                else bg = p.surface;
            }
            pFill.setColor(bg);
            c.drawRoundRect(tmp, Math.min(kw, kh) * 0.24f, Math.min(kw, kh) * 0.24f, pFill);

            pText.setTypeface(TF_MED);
            pText.setTextSize(digitSize);
            pText.setTextAlign(Paint.Align.CENTER);
            pText.setColor(done ? p.hint : p.entry);
            pText.setAlpha(done ? 72 : 255);
            drawTextV(c, String.valueOf(d), tmp.centerX(), tmp.centerY() + kh * 0.05f, pText);
            pText.setAlpha(255);

            if (left[d] > 0 && left[d] < 9) {
                pText.setTypeface(TF_REG);
                pText.setTextSize(badgeSize);
                pText.setTextAlign(Paint.Align.RIGHT);
                pText.setColor(Palette.alpha(p.hint, 0.85f));
                c.drawText(String.valueOf(left[d]), tmp.right - kw * 0.16f,
                        tmp.top + badgeSize * 1.45f, pText);
            }

            addHit(tmp, KIND_KEY, d, !done);
        }
    }

    // ================================================================== 数字键下方：回退 / 笔记

    /**
     * 数字键下面一排两个宽按钮：左「回退」、右「笔记」。
     *
     * 笔记按钮压在右下角，右手拇指不用横跨屏幕就能开关笔记模式；
     * 回退放在左侧对称位置，两只手都顺手。
     */
    private void drawActionBar(Canvas c, Palette p) {
        final float gap = 10 * f;
        final float bw = (rActs.width() - gap) / 2f;
        final float radius = Math.min(bw, rActs.height()) * 0.24f;

        drawActionBtn(c, p, IC_UNDO, "回退",
                rActs.left, rActs.top, rActs.left + bw, rActs.bottom,
                radius, g.hasUndo() && !g.done, false);

        drawActionBtn(c, p, IC_NOTES, g.notesMode ? "笔记中" : "笔记",
                rActs.right - bw, rActs.top, rActs.right, rActs.bottom,
                radius, !g.done, g.notesMode);
    }

    private void drawActionBtn(Canvas c, Palette p, int id, String label,
                               float l, float t, float r, float b, float radius,
                               boolean enabled, boolean on) {
        tmp.set(l, t, r, b);
        final boolean pressed = pressKind == KIND_ACTBAR && pressArg == id && enabled;

        int bg = p.surface;
        if (on) bg = Palette.mix(p.accent, p.bg, p.same);          // 笔记模式：与工具栏铅笔同一套高亮
        else if (pressed) bg = Palette.mix(p.accent, p.surface, 0.22f);
        pFill.setColor(bg);
        c.drawRoundRect(tmp, radius, radius, pFill);

        int col = on ? p.entry : (pressed ? p.text : p.hint);
        if (!enabled) col = Palette.alpha(p.hint, 0.135f);

        final float iconBox = 22 * f;
        final float textSize = 15 * f;
        final float gapIT = 8 * f;

        pText.setTypeface(TF_MED);
        pText.setTextSize(textSize);
        pText.setTextAlign(Paint.Align.LEFT);
        final float tw = pText.measureText(label);
        final float x0 = tmp.centerX() - (iconBox + gapIT + tw) / 2f;

        drawGlyph(c, ICON_NAMES[id], col, iconBox, x0 + iconBox / 2f, tmp.centerY());
        pText.setColor(col);
        drawTextV(c, label, x0 + iconBox + gapIT, tmp.centerY(), pText);

        addHit(tmp, KIND_ACTBAR, id, enabled);
    }

    // ================================================================== 浮层

    private void drawOverlay(Canvas c, Palette p) {
        if (overlay == OV_PAUSE) {          // 暂停浮层自己管遮罩和点击
            drawPause(c, p);
            return;
        }

        // 遮罩（只对菜单生效；卡片本身自带变暗）
        if (overlay == OV_CARD) {
            c.drawColor(Palette.alpha(0x000000, 0.55f));
        } else {
            // 点空白关闭
            Hit scrim = new Hit();
            scrim.r.set(0, 0, getWidth(), getHeight());
            scrim.kind = KIND_ACT;
            scrim.arg = ACT_CLOSE;
            scrim.pressable = false;
            hits.add(scrim);
        }

        if (overlay == OV_DIFF) drawDiffMenu(c, p);
        else if (overlay == OV_THEME) drawThemeMenu(c, p);
        else if (overlay == OV_CARD) drawCard(c, p);
    }

    /**
     * 暂停浮层：压暗整屏 + 居中卡片（标题 / 大号用时 / 三角恢复按钮）。
     *
     * 遮罩用 ACT_NOOP 而不是 ACT_CLOSE —— 点空白不关闭，必须点三角才继续，
     * 这样不会因为误触棋盘而"悄悄恢复"。
     */
    private void drawPause(Canvas c, Palette p) {
        c.drawColor(Palette.alpha(0x000000, 0.74f));

        Hit scrim = new Hit();
        scrim.r.set(0, 0, getWidth(), getHeight());
        scrim.kind = KIND_ACT;
        scrim.arg = ACT_NOOP;
        scrim.pressable = false;
        hits.add(scrim);

        final float padT = 26 * u, padB = 26 * u;
        final float titleH = 26 * u;
        final float gap1 = 12 * u;
        final float timeH = 56 * u;
        final float gap2 = 20 * u;
        final float btnD = 76 * u;
        final float ph = padT + titleH + gap1 + timeH + gap2 + btnD + padB;

        final float pw = Math.min(boardSize * 0.80f, getWidth() - 36 * f);
        final float cx = getWidth() / 2f;
        final float cy = getHeight() / 2f;
        rOvPanel.set(cx - pw / 2f, cy - ph / 2f, cx + pw / 2f, cy + ph / 2f);

        panel(c, p, rOvPanel, 20 * u);

        float y = rOvPanel.top + padT;

        pText.setTypeface(TF_MED);
        pText.setTextSize(15 * f);
        pText.setTextAlign(Paint.Align.CENTER);
        pText.setColor(p.hint);
        drawTextV(c, "已暂停", cx, y + titleH / 2f, pText);
        y += titleH + gap1;

        pText.setTypeface(TF_MED);
        pText.setTextSize(34 * f);
        pText.setColor(p.text);
        drawTextV(c, timeLabel(), cx, y + timeH / 2f, pText);
        y += timeH + gap2;

        // 三角恢复按钮
        final float by = y + btnD / 2f;
        tmp.set(cx - btnD / 2f, y, cx + btnD / 2f, y + btnD);
        final boolean pressed = pressKind == KIND_ACT && pressArg == ACT_RESUME;
        pFill.setStyle(Paint.Style.FILL);
        pFill.setColor(pressed ? Palette.mix(p.accent, 0xFFFFFFFF, 0.86f) : p.accent);
        c.drawCircle(cx, by, btnD / 2f, pFill);
        // 三角包围盒中心已经落在图标格中线上，直接居中画即可，与圆心对齐
        drawGlyph(c, "play", p.onAccent, btnD * 0.44f, cx, by);
        addHit(tmp, KIND_ACT, ACT_RESUME, true);
    }

    private void panel(Canvas c, Palette p, RectF r, float radius) {
        final int save = c.saveLayerAlpha(null, 255);
        pFill.setColor(p.bg);
        c.drawRoundRect(r, radius, radius, pFill);
        pStroke.setStyle(Paint.Style.STROKE);
        pStroke.setColor(Palette.alpha(p.text, 0.14f));
        pStroke.setStrokeWidth(1f);
        c.drawRoundRect(r, radius, radius, pStroke);
        c.restoreToCount(save);
    }

    private float menuRowH() {
        return 46 * f;
    }

    private void drawDiffMenu(Canvas c, Palette p) {
        final float rowH = menuRowH();
        final float pw = Math.max(150 * f, diffW + 30 * f);
        final float ph = rowH * DIFFS.length + 12 * f;
        float left = rTb1.right - pw;
        float top = rTb2.bottom + 8 * f;          // 落在两行工具栏之下，不遮图标
        if (top + ph > getHeight() - insetBottom) top = Math.max(insetTop + 8 * f, rTb1.top - ph - 8 * f);
        rOvPanel.set(left, top, left + pw, top + ph);

        panel(c, p, rOvPanel, 14 * f);

        pText.setTextSize(14.5f * f);
        pText.setTextAlign(Paint.Align.LEFT);
        for (int i = 0; i < DIFFS.length; i++) {
            final float ry = rOvPanel.top + 6 * f + i * rowH;
            tmp.set(rOvPanel.left + 6 * f, ry, rOvPanel.right - 6 * f, ry + rowH);
            final boolean cur = DIFFS[i].equals(g.diff);
            if (cur) {
                pFill.setColor(p.surface);
                c.drawRoundRect(tmp, 9 * f, 9 * f, pFill);
            }
            pText.setTypeface(cur ? TF_MED : TF_REG);
            pText.setColor(cur ? p.entry : p.text);
            drawTextV(c, DIFF_CN[i], tmp.left + 14 * f, tmp.centerY(), pText);
            addHit(tmp, KIND_ACT, ACT_DIFF + i, false);
        }
    }

    private void drawThemeMenu(Canvas c, Palette p) {
        final float rowH = menuRowH();
        final float pw = 176 * f;
        final int nTheme = Palette.THEMES.length;
        final float ph = rowH * (nTheme + 2) + 20 * f;

        float left = rTb2.right - pw;
        if (left < 8 * f) left = 8 * f;
        float top = rTb2.bottom + 8 * f;
        if (top + ph > getHeight() - insetBottom) top = Math.max(8 * f, rTb2.top - ph - 8 * f);
        rOvPanel.set(left, top, left + pw, top + ph);

        panel(c, p, rOvPanel, 14 * f);

        final String curTheme = Prefs.theme();
        final String curScheme = Prefs.scheme();

        pText.setTextSize(14.5f * f);
        pText.setTextAlign(Paint.Align.LEFT);
        float ry = rOvPanel.top + 6 * f;

        for (int i = 0; i < nTheme; i++) {
            tmp.set(rOvPanel.left + 6 * f, ry, rOvPanel.right - 6 * f, ry + rowH);
            final boolean cur = Palette.THEMES[i].id.equals(curTheme);
            if (cur) {
                pFill.setColor(p.surface);
                c.drawRoundRect(tmp, 9 * f, 9 * f, pFill);
            }
            pText.setTypeface(cur ? TF_MED : TF_REG);
            pText.setColor(cur ? p.entry : p.text);
            drawTextV(c, Palette.THEMES[i].name, tmp.left + 14 * f, tmp.centerY(), pText);
            addHit(tmp, KIND_ACT, ACT_THEME + i, false);
            ry += rowH;
        }

        // 分隔线
        pStroke.setStyle(Paint.Style.STROKE);
        pStroke.setColor(Palette.alpha(p.text, 0.12f));
        pStroke.setStrokeWidth(1f);
        c.drawLine(rOvPanel.left + 12 * f, ry + 5 * f, rOvPanel.right - 12 * f, ry + 5 * f, pStroke);
        ry += 10 * f;

        final String[] names = {"深色", "浅色"};
        final String[] ids = {"dark", "light"};
        for (int i = 0; i < 2; i++) {
            tmp.set(rOvPanel.left + 6 * f, ry, rOvPanel.right - 6 * f, ry + rowH);
            final boolean cur = ids[i].equals(curScheme);
            if (cur) {
                pFill.setColor(p.surface);
                c.drawRoundRect(tmp, 9 * f, 9 * f, pFill);
            }
            pText.setTypeface(cur ? TF_MED : TF_REG);
            pText.setColor(cur ? p.entry : p.text);
            drawTextV(c, names[i], tmp.left + 14 * f, tmp.centerY(), pText);
            addHit(tmp, KIND_ACT, ACT_SCHEME + i, false);
            ry += rowH;
        }
    }

    private void drawCard(Canvas c, Palette p) {
        final float padL = 20 * f, padT = 22 * f, padR = 20 * f, padB = 18 * f;
        final float pw = Math.min(300 * f, getWidth() - 48 * f);

        // 先估算高度
        boolean hasStats = cardK1 != null;
        float h = padT + 24 * f;                     // 标题
        if (!cardSub.isEmpty()) h += 8 * f + 18 * f; // 副标题
        if (hasStats) h += 16 * f + 46 * f + 16 * f;
        else h += 20 * f;
        h += 46 * f;                                 // 主按钮
        if (!cardSecondary.isEmpty()) h += 10 * f + 46 * f;
        h += padB;

        final float cx = getWidth() / 2f;
        final float cy = getHeight() / 2f;
        rOvPanel.set(cx - pw / 2f, cy - h / 2f, cx + pw / 2f, cy + h / 2f);

        panel(c, p, rOvPanel, 18 * f);

        float y = rOvPanel.top + padT;

        pText.setTypeface(TF_MED);
        pText.setTextSize(19 * f);
        pText.setTextAlign(Paint.Align.CENTER);
        pText.setColor(p.text);
        c.drawText(cardTitle, cx, y + 19 * f * 0.78f, pText);
        y += 26 * f;

        if (!cardSub.isEmpty()) {
            pText.setTypeface(TF_REG);
            pText.setTextSize(12.5f * f);
            pText.setColor(p.hint);
            c.drawText(cardSub, cx, y + 12 * f, pText);
            y += 24 * f;
        }

        if (hasStats) {
            y += 14 * f;
            final float colW = 90 * f;
            final float gap = 18 * f;
            final float total = colW * 2 + gap;
            float x0 = cx - total / 2f;
            drawStat(c, p, x0, y, colW, cardV1, cardK1);
            drawStat(c, p, x0 + colW + gap, y, colW, cardV2, cardK2);
            y += 46 * f;
        } else {
            y += 14 * f;
        }

        y += padB - 6 * f;

        // 主按钮
        tmp.set(rOvPanel.left + padL, y, rOvPanel.right - padR, y + 46 * f);
        final boolean prPressed = pressKind == KIND_ACT && pressArg == ACT_CARD_1;
        pFill.setColor(prPressed
                ? Palette.mix(p.accent, 0xFFFFFFFF, 0.86f)
                : p.accent);
        c.drawRoundRect(tmp, 12 * f, 12 * f, pFill);
        pText.setTypeface(TF_MED);
        pText.setTextSize(15 * f);
        pText.setTextAlign(Paint.Align.CENTER);
        pText.setColor(p.onAccent);
        drawTextV(c, cardPrimary, tmp.centerX(), tmp.centerY(), pText);
        addHit(tmp, KIND_ACT, ACT_CARD_1, true);
        y += 46 * f;

        if (!cardSecondary.isEmpty()) {
            y += 10 * f;
            tmp.set(rOvPanel.left + padL, y, rOvPanel.right - padR, y + 46 * f);
            pFill.setColor(p.surface);
            c.drawRoundRect(tmp, 12 * f, 12 * f, pFill);
            pText.setTypeface(TF_MED);
            pText.setTextSize(15 * f);
            pText.setColor(p.entry);
            drawTextV(c, cardSecondary, tmp.centerX(), tmp.centerY(), pText);
            addHit(tmp, KIND_ACT, ACT_CARD_2, true);
        }
    }

    private void drawStat(Canvas c, Palette p, float x, float y, float w, String val, String cap) {
        pText.setTypeface(TF_MED);
        pText.setTextSize(19 * f);
        pText.setTextAlign(Paint.Align.CENTER);
        pText.setColor(p.text);
        c.drawText(val == null ? "" : val, x + w / 2f, y + 19 * f * 0.8f, pText);

        pText.setTypeface(TF_REG);
        pText.setTextSize(11 * f);
        pText.setColor(p.hint);
        c.drawText(cap == null ? "" : cap, x + w / 2f, y + 19 * f * 0.8f + 17 * f, pText);
    }

    // ================================================================== 文字 / 图标工具

    private Paint solid(int color) {
        pFill.setColor(color);
        pFill.setStyle(Paint.Style.FILL);
        return pFill;
    }

    /** 以 cy 为垂直中心写字（水平对齐由 paint.textAlign 决定） */
    private void drawTextV(Canvas c, String s, float x, float cy, Paint pt) {
        pt.getFontMetrics(fm);
        final float baseline = cy - (fm.ascent + fm.descent) / 2f;
        c.drawText(s, x, baseline, pt);
    }

    private Hit addHit(RectF r, int kind, int arg, boolean pressable) {
        Hit h = new Hit();
        h.r.set(r);
        h.kind = kind;
        h.arg = arg;
        h.pressable = pressable;
        hits.add(h);
        return h;
    }

    /**
     * 矢量图标：坐标写在一个 24×24 的格子里，整体缩放到 box 大小绘制。
     * 与 Qt 的 paintIcon() 逐条路径对应。
     */
    private void drawGlyph(Canvas c, String name, int color, float box, float cx, float cy) {
        final float s = box / 24f;
        final int save = c.save();
        c.translate(cx - box / 2f, cy - box / 2f);
        c.scale(s, s);

        pIcon.setColor(color);
        pIcon.setStyle(Paint.Style.STROKE);
        pIcon.setStrokeCap(Paint.Cap.ROUND);
        pIcon.setStrokeJoin(Paint.Join.ROUND);
        pIcon.setStrokeWidth(Math.max(1.7f, 2.2f / s));

        switch (name) {
            case "clock":
                pIcon.setStyle(Paint.Style.STROKE);
                c.drawCircle(12, 12, 9, pIcon);
                c.drawLine(12, 7.5f, 12, 12, pIcon);
                c.drawLine(12, 12, 15, 13.8f, pIcon);
                break;

            case "cross":
                c.drawLine(6, 6, 18, 18, pIcon);
                c.drawLine(18, 6, 6, 18, pIcon);
                break;

            case "undo":
                path.reset();
                path.moveTo(3, 8);
                path.lineTo(3, 13);
                path.lineTo(8, 13);
                c.drawPath(path, pIcon);
                path.reset();
                path.moveTo(5.5f, 11);
                path.cubicTo(8, 6.2f, 13.5f, 5.2f, 16.8f, 8.4f);
                path.cubicTo(19.6f, 11.2f, 18.6f, 15.4f, 15, 17);
                c.drawPath(path, pIcon);
                break;

            case "pencil":
                c.drawLine(12, 20.4f, 21, 20.4f, pIcon);
                path.reset();
                path.moveTo(16.6f, 3.6f);
                path.lineTo(20.4f, 7.4f);
                path.lineTo(7, 20.8f);
                path.lineTo(3, 21.0f);
                path.lineTo(3.2f, 17.0f);
                path.close();
                c.drawPath(path, pIcon);
                c.drawLine(14.2f, 6.0f, 18.0f, 9.8f, pIcon);
                break;

            case "eraser":
                c.drawLine(8, 20.4f, 20, 20.4f, pIcon);
                path.reset();
                path.moveTo(4.6f, 15.4f);
                path.lineTo(11.4f, 8.6f);
                path.cubicTo(12.2f, 7.8f, 13.4f, 7.8f, 14.2f, 8.6f);
                path.lineTo(17.6f, 12.0f);
                path.cubicTo(18.4f, 12.8f, 18.4f, 14.0f, 17.6f, 14.8f);
                path.lineTo(12.8f, 19.2f);
                path.lineTo(7.3f, 19.2f);
                path.close();
                c.drawPath(path, pIcon);
                c.drawLine(8.2f, 11.8f, 13.8f, 17.4f, pIcon);
                break;

            case "bulb":
                c.drawLine(9.2f, 18.0f, 14.8f, 18.0f, pIcon);
                c.drawLine(10.3f, 21.0f, 13.7f, 21.0f, pIcon);
                path.reset();
                path.moveTo(12, 3);
                path.cubicTo(8.7f, 3, 6.0f, 5.7f, 6.0f, 9.0f);
                path.cubicTo(6.0f, 11.0f, 7.0f, 12.2f, 8.0f, 13.4f);
                path.cubicTo(8.7f, 14.3f, 9.0f, 15.0f, 9.0f, 16.0f);
                path.lineTo(15.0f, 16.0f);
                path.cubicTo(15.0f, 15.0f, 15.3f, 14.3f, 16.0f, 13.4f);
                path.cubicTo(17.0f, 12.2f, 18.0f, 11.0f, 18.0f, 9.0f);
                path.cubicTo(18.0f, 5.7f, 15.3f, 3, 12, 3);
                c.drawPath(path, pIcon);
                break;

            case "plus":
                c.drawLine(12, 5, 12, 19, pIcon);
                c.drawLine(5, 12, 19, 12, pIcon);
                break;

            case "palette":
                c.drawCircle(12, 12, 9, pIcon);
                pIcon.setStyle(Paint.Style.FILL);
                c.drawCircle(9.2f, 9.2f, 1.15f, pIcon);
                c.drawCircle(14.8f, 9.2f, 1.15f, pIcon);
                c.drawCircle(9.6f, 14.6f, 1.15f, pIcon);
                break;

            case "play":
                // 实心三角：暂停浮层的"继续"按钮、以及暂停态的计时器胶囊
                // 三角取 6.8~17.2，包围盒中心正好落在 24 格的中线 12 上，
                // 这样放进圆形按钮时三角中心＝圆心，不用再额外偏移
                pIcon.setStyle(Paint.Style.FILL);
                path.reset();
                path.moveTo(6.8f, 5.0f);
                path.lineTo(17.2f, 12f);
                path.lineTo(6.8f, 19.0f);
                path.close();
                c.drawPath(path, pIcon);
                break;

            case "chevron":
                c.drawLine(6.5f, 9.5f, 12, 15, pIcon);
                c.drawLine(12, 15, 17.5f, 9.5f, pIcon);
                break;

            default:
                break;
        }
        c.restoreToCount(save);
    }

    // ================================================================== 触摸

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        final float x = e.getX(), y = e.getY();

        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                Hit h = findHit(x, y);
                pressKind = h == null ? -1 : h.kind;
                pressArg = h == null ? -1 : h.arg;
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE: {
                Hit h = findHit(x, y);
                int k = h == null ? -1 : h.kind;
                int a = h == null ? -1 : h.arg;
                if (k != pressKind || a != pressArg) {
                    pressKind = k;
                    pressArg = a;
                    invalidate();
                }
                return true;
            }
            case MotionEvent.ACTION_UP: {
                Hit h = findHit(x, y);
                pressKind = -1;
                pressArg = -1;
                if (h != null) {
                    activate(h);
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                pressKind = -1;
                pressArg = -1;
                invalidate();
                return true;
        }
        return super.onTouchEvent(e);
    }

    private Hit findHit(float x, float y) {
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (h.r.contains(x, y)) return h;
        }
        return null;
    }

    private void activate(Hit h) {
        // 计时器胶囊：任何状态下都能点开暂停浮层（暂停后用返回键关掉浮层，也靠它再叫回来）
        if (h.kind == KIND_TIME) {
            openPause();
            return;
        }
        if (h.kind == KIND_ACT && h.arg == ACT_RESUME) {
            resumeGame();
            return;
        }
        if (h.kind == KIND_ACT && h.arg == ACT_NOOP) return;   // 暂停浮层的空白处：什么都不做
        if (g.paused) return;                                  // 暂停期间其余输入一律忽略

        switch (h.kind) {
            case KIND_BOARD:
                g.sel = (g.sel == h.arg) ? -1 : h.arg;
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                invalidate();
                break;

            case KIND_KEY:
                if (g.sel < 0) {                       // 没选格子时先闪一下提示
                    flashNoSelection();
                    return;
                }
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                boolean wrong = g.place(h.arg);
                afterChange(wrong);
                break;

            case KIND_ICON:
                onIcon(h.arg);
                break;

            case KIND_ACTBAR:                      // 回退 / 笔记，逻辑与工具栏图标完全一致
                if (h.pressable) onIcon(h.arg);
                break;

            case KIND_DIFF:
                overlay = OV_DIFF;
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                invalidate();
                break;

            case KIND_ACT:
                onAction(h.arg);
                break;
        }
    }

    private void onIcon(int id) {
        switch (id) {
            case IC_UNDO:
                if (g.undo()) { haptic(HapticFeedbackConstants.KEYBOARD_TAP); afterChange(false); }
                break;
            case IC_NOTES:
                g.notesMode = !g.notesMode;
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                invalidate();
                break;
            case IC_ERASE:
                g.erase();
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                afterChange(false);
                break;
            case IC_HINT:
                if (g.hint() >= 0) { haptic(HapticFeedbackConstants.KEYBOARD_TAP); afterChange(false); }
                break;
            case IC_NEW:
                askNewGame();
                break;
            case IC_THEME:
                overlay = OV_THEME;
                haptic(HapticFeedbackConstants.KEYBOARD_TAP);
                invalidate();
                break;
        }
    }

    private void onAction(int act) {
        if (act == ACT_CLOSE) {
            overlay = OV_NONE;
            invalidate();
            return;
        }
        if (act == ACT_CARD_1) {
            overlay = OV_NONE;
            if (cardAction != null) cardAction.run();
            invalidate();
            return;
        }
        if (act == ACT_CARD_2) {
            overlay = OV_NONE;
            invalidate();
            return;
        }
        if (act >= ACT_DIFF && act < ACT_DIFF + 4) {
            final String d = DIFFS[act - ACT_DIFF];
            overlay = OV_NONE;
            g.newGame(d);
            Prefs.saveGame(g);
            invalidate();
            if (cb != null) cb.onStateChanged();
            return;
        }
        if (act >= ACT_THEME && act < ACT_THEME + Palette.THEMES.length) {
            Prefs.setTheme(Palette.THEMES[act - ACT_THEME].id);
            Prefs.applyTheme();
            overlay = OV_NONE;
            invalidate();
            if (cb != null) cb.onChangeSkin();
            return;
        }
        if (act == ACT_SCHEME || act == ACT_SCHEME + 1) {
            Prefs.setScheme(act == ACT_SCHEME ? "dark" : "light");
            Prefs.applyTheme();
            overlay = OV_NONE;
            invalidate();
            if (cb != null) cb.onChangeSkin();
        }
    }

    private void afterChange(boolean wrong) {
        if (wrong) haptic(HapticFeedbackConstants.LONG_PRESS);
        if (!g.done && g.solved()) {
            g.done = true;
            g.running = false;
            Prefs.saveGame(g);
            invalidate();
            showWin();
            return;
        }
        notifyChanged();
    }

    private void haptic(int kind) {
        try {
            performHapticFeedback(kind);
        } catch (Throwable ignored) {
        }
    }

    /** 没选格子就按数字键：让棋盘外框闪一下 accent 色，提示"先点格子" */
    private void flashNoSelection() {
        noSelFlashUntil = System.currentTimeMillis() + 320;
        haptic(HapticFeedbackConstants.LONG_PRESS);
        invalidate();
        postInvalidateDelayed(340);
    }

    // ================================================================== 对话框

    private void askNewGame() {
        if (!g.started()) {
            g.newGame(g.diff);
            Prefs.saveGame(g);
            invalidate();
            if (cb != null) cb.onStateChanged();
            return;
        }
        cardTitle = "开始新局？";
        cardSub = "当前进度会被清掉。";
        cardK1 = null;
        cardK2 = null;
        cardPrimary = "开始新局";
        cardSecondary = "继续这局";
        cardAction = () -> {
            g.newGame(g.diff);
            Prefs.saveGame(g);
            invalidate();
            if (cb != null) cb.onStateChanged();
        };
        overlay = OV_CARD;
        invalidate();
    }

    private void showWin() {
        cardTitle = "完成";
        cardSub = "全部填对，漂亮。";
        cardK1 = "用时";
        cardV1 = timeLabel();
        cardK2 = "错误";
        cardV2 = String.valueOf(g.mistakes);
        cardPrimary = "再来一局";
        cardSecondary = "看看棋盘";
        cardAction = () -> {
            g.newGame(g.diff);
            Prefs.saveGame(g);
            invalidate();
            if (cb != null) cb.onStateChanged();
        };
        overlay = OV_CARD;
        invalidate();
    }

    // ================================================================== 外部调用

    public void startNewGame(String diff) {
        g.newGame(diff);
        Prefs.saveGame(g);
        invalidate();
        if (cb != null) cb.onStateChanged();
    }

    public boolean hasOverlay() {
        return overlay != OV_NONE;
    }

    public void closeOverlay() {
        if (overlay != OV_NONE) {
            overlay = OV_NONE;
            invalidate();
        }
    }

    // ================================================================== 暂停

    /**
     * 两种暂停要分开处理：
     *   后台自动暂停（autoPaused=true）—— 回到前台自动继续，不弹浮层；
     *   用户主动暂停（点计时器）—— 必须点三角才继续。
     */
    private boolean autoPaused = false;

    /** 打开暂停浮层：点左上角计时器胶囊走到这里（顺带把计时真正停住） */
    public void openPause() {
        if (g.done) return;
        final boolean was = g.paused;
        g.paused = true;
        autoPaused = false;          // 用户主动暂停：回来不自动继续
        overlay = OV_PAUSE;
        haptic(HapticFeedbackConstants.KEYBOARD_TAP);
        invalidate();
        if (!was && cb != null) cb.onStateChanged();
    }

    /** 切到后台：静默暂停，不改变当前浮层（用户主动暂停的浮层保留） */
    public void autoPause() {
        if (g.done || g.paused) return;
        g.paused = true;
        autoPaused = true;
        invalidate();
        if (cb != null) cb.onStateChanged();
    }

    /** 回到前台：只有"后台自动暂停"才自动继续；用户主动暂停的必须自己点三角 */
    public void autoResume() {
        if (!autoPaused) return;
        autoPaused = false;
        g.paused = false;
        overlay = OV_NONE;
        invalidate();
        if (cb != null) cb.onStateChanged();
    }

    /** 只有三角形按钮能走到这里 */
    private void resumeGame() {
        autoPaused = false;
        if (!g.paused) {
            overlay = OV_NONE;
            invalidate();
            return;
        }
        g.paused = false;
        overlay = OV_NONE;
        haptic(HapticFeedbackConstants.KEYBOARD_TAP);
        invalidate();
        if (cb != null) cb.onStateChanged();
    }
}
