package com.buddy.sudoku;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

/**
 * 单 Activity + 全自绘 View。
 *
 * 适配要点（OPPO Find X8s：2640×1216 / 6.32" / 460ppi / 19.5:9 / 中置挖孔 / ColorOS 15）：
 *  - 竖屏锁定，避免旋转后棋盘尺寸突变
 *  - 全面屏 edge-to-edge：开 setDecorFitsSystemWindows(false)，再自己吃系统栏 + 挖孔的安全区
 *  - 状态栏/导航栏全透明，图标颜色跟着主题明暗切换
 *  - 棋盘宽度取「屏宽 - 28dp」，其余控件全部按棋盘宽度归一化，任何 DPI 都等比
 */
public class MainActivity extends Activity implements SudokuView.Callback {

    private final Game game = new Game();
    private final Handler ui = new Handler(Looper.getMainLooper());

    private SudokuView view;
    private boolean resumed = false;
    private int saveCountdown = 10;

    private final Runnable ticker = new Runnable() {
        @Override
        public void run() {
            if (game.running && !game.done && !game.paused) {
                game.seconds++;
                if (--saveCountdown <= 0) {
                    saveCountdown = 10;
                    Prefs.saveGame(game);
                }
                if (view != null) view.invalidate();
            }
            ui.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);

        Prefs.init(this);
        Prefs.applyTheme();

        Window w = getWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            w.setDecorFitsSystemWindows(false);
        }
        w.setBackgroundDrawable(new ColorDrawable(Palette.cur.bg));

        if (!Prefs.loadGame(game)) {
            game.newGame("medium");
        }

        view = new SudokuView(this, game);
        view.setCallback(this);
        view.setBackgroundColor(Palette.cur.bg);
        view.setOnApplyWindowInsetsListener((v, insets) -> {
            Insets i = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            ((SudokuView) v).setSystemInsets(i.top, i.bottom);
            return insets;
        });
        setContentView(view);

        // 必须在 setContentView 之后：此前 DecorView 尚未创建，
        // PhoneWindow.getInsetsController() 内部会对 null 的 DecorView 取方法而抛 NPE。
        applyBarIcons();
    }

    /**
     * 状态栏 / 导航栏图标颜色：深色背景用白图标，浅色背景用深图标。
     *
     * 注意：不能直接用 getWindow().getInsetsController()——在 DecorView 未创建时
     * 框架内部会 NPE（而非返回 null）。这里走 getDecorView() 保证装饰视图存在，
     * 整体再加兜底，系统栏设置失败绝不能拖垮启动。
     */
    private void applyBarIcons() {
        final Window w = getWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                WindowInsetsController c = w.getDecorView().getWindowInsetsController();
                if (c != null) {
                    final int flags = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                            | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
                    c.setSystemBarsAppearance(Prefs.isLightScheme() ? flags : 0, flags);
                }
            } catch (Throwable ignored) {
                // 部分 ROM 在窗口未 attach 时会抛，忽略即可，不影响功能
            }
        }
        try {
            w.setStatusBarColor(Color.TRANSPARENT);
            w.setNavigationBarColor(Color.TRANSPARENT);
        } catch (Throwable ignored) {
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        ui.removeCallbacks(ticker);
        ui.postDelayed(ticker, 1000);
        // 回来时：后台自动暂停的直接续上（不弹浮层）；用户主动暂停的保持原样
        if (view != null) view.autoResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        resumed = false;
        ui.removeCallbacks(ticker);
        // 切到后台 / 锁屏 → 静默暂停；用户主动暂停的不动它的浮层
        if (view != null) view.autoPause();
        Prefs.saveGame(game);
    }

    @Override
    public void onBackPressed() {
        if (view != null && view.hasOverlay()) {
            view.closeOverlay();
            return;
        }
        super.onBackPressed();
    }

    // ------------------------------------------------------------ SudokuView.Callback

    @Override
    public void onStateChanged() {
        Prefs.saveGame(game);
    }

    @Override
    public void onChangeSkin() {
        applyBarIcons();
        getWindow().setBackgroundDrawable(new ColorDrawable(Palette.cur.bg));
        if (view != null) view.setBackgroundColor(Palette.cur.bg);
    }
}
