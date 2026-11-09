#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QSettings>
#include <QTimer>
#include <QVector>
#include <QSize>

#include "game.h"
#include "widgets.h"

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);
    void demoState();          // 仅用于截屏自检
    void demoNotes();          // 仅用于「笔记」讲解截图
    void setScale(qreal k);    // 按比例设置窗口尺寸（k=1 为设计最大尺寸）

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;
    bool eventFilter(QObject *obj, QEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void keyPressEvent(QKeyEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
    void tick();
    void onDiffChanged(const QString &k);
    void onWinMin();
    void onWinReset();
    void onWinClose();

private:
    void buildUi();
    void relayout();
    void refresh();
    void afterChange();
    void place(int d);
    void eraseCell();
    void undoMove();
    void toggleNotes();
    void hintCell();
    void newGame(const QString &diff = QString());
    void askNewGame();
    void showWin();
    void pauseGame(bool showNow = true);   // 置暂停位；showNow=true 时立刻弹暂停界面
    void resumeGame();
    void showPause();
    QString timeText() const;
    void openThemeMenu();
    void applyTheme();
    void saveState();
    void loadState();
    QVector<int> readArr(const QString &key, const QVector<int> &def);

    qreal minScale() const { return m_framed ? FRAMED_MIN_SCALE : MIN_SCALE; }
    void applyChrome();        // 按当前模式设置最小尺寸 / 圆角遮罩
    void setFramed(bool framed);
    int hitEdge(const QPoint &p) const;   // 1=左 2=右 4=上 8=下

    Game m_game;
    QSettings m_settings;
    QTimer m_timer;

    Pill *m_pillTime = nullptr;
    Pill *m_pillMiss = nullptr;
    DiffBtn *m_diff = nullptr;
    IconBtn *m_bUndo = nullptr;
    IconBtn *m_bNotes = nullptr;
    IconBtn *m_bErase = nullptr;
    IconBtn *m_bHint = nullptr;
    IconBtn *m_bNew = nullptr;
    IconBtn *m_bTheme = nullptr;
    Board *m_board = nullptr;
    QVector<KeyBtn *> m_keys;
    CapBtn *m_bWinMin = nullptr;
    CapBtn *m_bWinReset = nullptr;
    CapBtn *m_bWinClose = nullptr;

    QSize m_last;          // 上次锁定后的尺寸，用于判断拖拽方向
    bool m_locking = false;
    bool m_wasActive = false;   // 曾经激活过（避免启动瞬间被误判为"离开"）
    bool m_pauseShowing = false;
    bool m_closing = false;     // 正在关闭：屏蔽"失活 → 暂停"的收尾误触发
    bool m_autoPaused = false;  // 暂停来自"离开窗口"：回到窗口时自动继续
    qreal m_k = 1.0;       // 当前缩放系数
    bool m_framed = false; // false = 无边框自绘（可缩到棋盘 80×80）
    int m_edge = 0;        // 正在拖拽的边缘
    QPoint m_pressGlobal;
    QRect m_pressGeom;
};

#endif // MAINWINDOW_H
