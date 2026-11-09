#include "mainwindow.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QRegion>
#include <QMenu>
#include <QAction>
#include <QCoreApplication>
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QResizeEvent>
#include <QEvent>
#include <QtMath>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

/* 绿色单文件版：优先把 settings.ini 放在 exe 同目录（便携）；
   若 exe 所在目录不可写（Program Files / 只读介质），回退到 %APPDATA%\...\Sudoku\ */
static QString settingsFilePath()
{
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini");
    QFile probe(local);
    if (probe.open(QIODevice::ReadWrite)) {          // 已存在则可读写；不存在则创建
        probe.close();
        return local;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/settings.ini");
}

Window::Window(QWidget *parent)
    : QWidget(parent), m_settings(settingsFilePath(), QSettings::IniFormat)
{
    setWindowTitle(QStringLiteral("数独"));
    m_framed = m_settings.value(QStringLiteral("ui/framed"), false).toBool();
    setWindowFlag(Qt::FramelessWindowHint, !m_framed);   // 无边框才能缩到棋盘 80×80
    setMouseTracking(true);
    applyTheme();
    buildUi();

    // 最大 432×522（设计尺寸）；最小见 minScale()：无边框 86×104 / 带标题栏 120×145
    setMaximumSize(qRound(DESIGN_W), qRound(DESIGN_H));
    applyChrome();
    m_last = QSize(qRound(DESIGN_W), qRound(DESIGN_H));
    resize(m_last);

    loadState();
    refresh();
    relayout();

    // 恢复上次的窗口尺寸（按比例换算，防止旧配置越界）
    bool restored = false;
    const QRect g = m_settings.value(QStringLiteral("ui/geometry")).toRect();
    if (g.width() > 0 && g.height() > 0) {
        qreal k = qMin(g.width() / DESIGN_W, g.height() / DESIGN_H);
        k = qBound(minScale(), k, 1.0);
        resize(qRound(DESIGN_W * k), qRound(DESIGN_H * k));
        m_last = size();
        relayout();
        applyChrome();
        const QRect want(g.topLeft(), size());
        if (QScreen *scr = screen()) {
            if (scr->availableGeometry().intersects(want)) {
                move(want.topLeft());
                restored = true;
            }
        }
    }

    connect(&m_timer, &QTimer::timeout, this, &Window::tick);
    m_timer.start(1000);

    if (!restored) {
        if (QScreen *scr = screen()) {
            const QRect a = scr->availableGeometry();
            move(a.center() - QPoint(width() / 2, height() / 2));
        }
    }
}

void Window::setScale(qreal k)
{
    k = qBound(minScale(), k, 1.0);
    m_last = QSize(qRound(DESIGN_W * k), qRound(DESIGN_H * k));
    m_locking = true;
    resize(m_last);
    m_locking = false;
    m_last = size();   // 被窗口管理器夹取时，以实际尺寸为准
    relayout();
}

void Window::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_framed) {                       // 有系统标题栏时用直角，交给系统
        p.fillRect(rect(), pal().bg);
        return;
    }
    const qreal r = qBound(3.0, 12.0 * m_k, 12.0);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    p.fillPath(path, pal().bg);
    p.setPen(QPen(col(pal().hint, 0.28), 1.0));
    p.drawPath(path);
}

/* 无边框模式下用圆角遮罩切掉四角；带标题栏模式直接用直角 */
void Window::applyChrome()
{
    setMinimumSize(qRound(DESIGN_W * minScale()), qRound(DESIGN_H * minScale()));
    if (m_framed) {
        clearMask();
        return;
    }
    const qreal r = qBound(3.0, 12.0 * m_k, 12.0);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), r, r);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void Window::setFramed(bool framed)
{
    if (framed == m_framed) return;
    m_framed = framed;
    m_settings.setValue(QStringLiteral("ui/framed"), m_framed);
    setWindowFlag(Qt::FramelessWindowHint, !m_framed);
    show();                                   // 改窗口标志后必须重新 show
    applyChrome();
    // 带标题栏时把尺寸抬到 120×145 以上，避免被 WM 回弹破坏比例
    if (m_framed) {
        const qreal k = qMax(m_k, minScale());
        resize(qRound(DESIGN_W * k), qRound(DESIGN_H * k));
    }
    m_last = size();
    relayout();
    applyChrome();
    update();
}

int Window::hitEdge(const QPoint &p) const
{
    if (m_framed) return 0;                   // 带标题栏时用系统的边框
    const int b = qMax(2, qRound(6 * m_k));
    int e = 0;
    if (p.x() <= b)                 e |= 1;
    else if (p.x() >= width() - 1 - b) e |= 2;
    if (p.y() <= b)                 e |= 4;
    else if (p.y() >= height() - 1 - b) e |= 8;
    return e;
}

void Window::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    m_edge = hitEdge(e->position().toPoint());
    if (m_edge) {                             // 边缘：手动等比缩放
        m_pressGlobal = e->globalPosition().toPoint();
        m_pressGeom = geometry();
        return;
    }
    // 其他空白处：拖动窗口（Qt 原生拖动，支持贴边吸附）
    if (QWindow *h = windowHandle()) { if (h->startSystemMove()) return; }
    m_pressGlobal = e->globalPosition().toPoint();
    m_pressGeom = geometry();
    m_edge = -1;                              // -1 = 手动移动
    QWidget::mousePressEvent(e);
}

void Window::mouseMoveEvent(QMouseEvent *e)
{
    if (m_edge == -1) {                       // 手动移动
        const QPoint d = e->globalPosition().toPoint() - m_pressGlobal;
        move(m_pressGeom.topLeft() + d);
        return;
    }
    if (!m_edge) {
        const int hit = hitEdge(e->position().toPoint());
        Qt::CursorShape c = Qt::ArrowCursor;
        if (hit == 1 || hit == 2)      c = Qt::SizeHorCursor;
        else if (hit == 4 || hit == 8) c = Qt::SizeVerCursor;
        else if (hit == 5 || hit == 10) c = Qt::SizeFDiagCursor;
        else if (hit == 6 || hit == 9) c = Qt::SizeBDiagCursor;
        setCursor(c);
        QWidget::mouseMoveEvent(e);
        return;
    }

    // 等比缩放：先按被拖的方向反解 k，再锚定对侧边
    const QPoint d = e->globalPosition().toPoint() - m_pressGlobal;
    const QRect g = m_pressGeom;
    int w = g.width(), h = g.height();
    if (m_edge & 1) w = g.width() - d.x();
    if (m_edge & 2) w = g.width() + d.x();
    if (m_edge & 4) h = g.height() - d.y();
    if (m_edge & 8) h = g.height() + d.y();

    const bool onlyLR = (m_edge & 3) && !(m_edge & 12);
    const bool onlyTB = (m_edge & 12) && !(m_edge & 3);
    double k = onlyLR ? w / double(DESIGN_W)
             : onlyTB ? h / double(DESIGN_H)
                      : qMin(w / double(DESIGN_W), h / double(DESIGN_H));
    k = qBound(double(minScale()), k, 1.0);

    const int nw = qRound(DESIGN_W * k), nh = qRound(DESIGN_H * k);
    int x = (m_edge & 1) ? g.right() + 1 - nw : g.x();
    int y = (m_edge & 4) ? g.bottom() + 1 - nh : g.y();

    m_locking = true;
    setGeometry(x, y, nw, nh);
    m_locking = false;
    m_last = size();
    relayout();
    applyChrome();
}

void Window::mouseReleaseEvent(QMouseEvent *e)
{
    m_edge = 0;
    m_settings.setValue(QStringLiteral("ui/geometry"), geometry());
    QWidget::mouseReleaseEvent(e);
}

void Window::leaveEvent(QEvent *e)
{
    if (!m_edge) setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(e);
}

void Window::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu m(this);
    m.setStyleSheet(menuQss());
    QAction *aNew = m.addAction(QStringLiteral("新局"));
    m.addSeparator();
    QAction *aTop = m.addAction(QStringLiteral("窗口置顶"));
    aTop->setCheckable(true);
    aTop->setChecked(windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QAction *aMin = m.addAction(QStringLiteral("最小化"));
    QAction *aFramed = m.addAction(QStringLiteral("显示系统标题栏"));
    aFramed->setCheckable(true);
    aFramed->setChecked(m_framed);
    m.addSeparator();
    QAction *aClose = m.addAction(QStringLiteral("关闭"));
    QAction *r = m.exec(e->globalPos());
    if (r == aNew)         askNewGame();
    else if (r == aMin)    showMinimized();
    else if (r == aClose)  close();
    else if (r == aFramed) setFramed(aFramed->isChecked());
    else if (r == aTop) {
        setWindowFlag(Qt::WindowStaysOnTopHint, aTop->isChecked());
        show();                    // 改标志后必须重新 show
        applyChrome();
        relayout();
    }
}

void Window::buildUi()
{
    m_pillTime = new Pill("clock", "00:00", this);
    m_pillTime->setCursor(Qt::PointingHandCursor);
    m_pillTime->setToolTip(QStringLiteral("点击暂停"));
    m_pillTime->installEventFilter(this);      // 点它 → 暂停界面
    m_pillMiss = new Pill("cross", "0", this);
    m_diff = new DiffBtn(this);
    m_diff->onChange = [this](const QString &k) { onDiffChanged(k); };

    m_bUndo = new IconBtn("undo", QStringLiteral("撤销 (Ctrl+Z)"), this);
    m_bNotes = new IconBtn("pencil", QStringLiteral("笔记 (N)"), this);
    m_bErase = new IconBtn("eraser", QStringLiteral("擦除 (Delete)"), this);
    m_bHint = new IconBtn("bulb", QStringLiteral("提示 (H)"), this);
    m_bNew = new IconBtn("plus", QStringLiteral("新局"), this);
    m_bTheme = new IconBtn("palette", QStringLiteral("配色"), this);

    connect(m_bUndo, &IconBtn::clicked, this, &Window::undoMove);
    connect(m_bNotes, &IconBtn::clicked, this, &Window::toggleNotes);
    connect(m_bErase, &IconBtn::clicked, this, &Window::eraseCell);
    connect(m_bHint, &IconBtn::clicked, this, &Window::hintCell);
    connect(m_bNew, &IconBtn::clicked, this, &Window::askNewGame);
    connect(m_bTheme, &IconBtn::clicked, this, &Window::openThemeMenu);

    m_board = new Board(&m_game, this);
    m_board->onChanged = [this] { refresh(); };

    // 顶部窄条：最小化 / 复位默认大小 / 关闭
    m_bWinMin = new CapBtn("winmin", QStringLiteral("最小化"), false, this);
    m_bWinReset = new CapBtn("winreset", QStringLiteral("恢复默认大小"), false, this);
    m_bWinClose = new CapBtn("winclose", QStringLiteral("关闭"), true, this);
    connect(m_bWinMin, &CapBtn::clicked, this, &Window::onWinMin);
    connect(m_bWinReset, &CapBtn::clicked, this, &Window::onWinReset);
    connect(m_bWinClose, &CapBtn::clicked, this, &Window::onWinClose);

    for (int d = 1; d <= 9; ++d) {
        auto *k = new KeyBtn(d, this);
        connect(k, &KeyBtn::clicked, this, [this, d] { place(d); });
        m_keys << k;
    }
}

/* 等比缩放布局：控件尺寸 = 设计尺寸 × k，位置用同一 k 排布 */
void Window::relayout()
{
    if (!m_board || !m_pillTime) return;

    qreal k = qMin(width() / DESIGN_W, height() / DESIGN_H);
    k = qBound(MIN_SCALE, k, 1.0);
    m_k = k;

    m_pillTime->applyScale(k);
    m_pillMiss->applyScale(k);
    m_diff->applyScale(k);
    for (IconBtn *b : {m_bUndo, m_bNotes, m_bErase, m_bHint, m_bNew, m_bTheme})
        b->applyScale(k);
    m_board->applyScale(k);
    for (KeyBtn *b : m_keys)
        b->applyScale(k);
    for (CapBtn *b : {m_bWinMin, m_bWinReset, m_bWinClose})
        b->applyScale(k);

    const int pad = qRound(16 * k);
    const int capH = qRound(CAP_H * k);
    const int top = capH + qRound(14 * k);
    const int gap = qRound(10 * k);
    const int sp = qMax(1, qRound(4 * k));
    const int barH = qRound(BAR_H * k);

    // 顶行：最小化 / 复位默认大小 / 关闭（右对齐，留出左侧拖动区）
    int cx = width() - pad;
    for (CapBtn *b : {m_bWinClose, m_bWinReset, m_bWinMin}) {
        cx -= b->width();
        b->move(cx, qRound((capH - b->height()) / 2.0));
        cx -= sp;
    }

    // 第一行：时间 / 错误 / 难度 …… 图标按钮右对齐
    int x = pad;
    m_pillTime->move(x, top + (barH - m_pillTime->height()) / 2);
    x += m_pillTime->width() + sp;
    m_pillMiss->move(x, top + (barH - m_pillMiss->height()) / 2);
    x += m_pillMiss->width() + sp;
    m_diff->move(x, top + (barH - m_diff->height()) / 2);

    int right = width() - pad;
    for (IconBtn *b : {m_bTheme, m_bNew, m_bHint, m_bErase, m_bNotes, m_bUndo}) {
        right -= b->width();
        b->move(right, top + (barH - b->height()) / 2);
        right -= sp;
    }

    // 棋盘
    const int boardY = top + barH + gap;
    m_board->move(pad, boardY);

    // 键盘：与棋盘同宽，9 等分
    const int keyY = boardY + m_board->height() + gap;
    const int kw = qMax(2, (m_board->width() - sp * 8) / 9);
    int kx = pad;
    for (KeyBtn *b : m_keys) {
        b->setFixedWidth(kw);
        b->move(kx, keyY);
        kx += kw + sp;
    }
}

/* 锁比例：拖任意边/角都等比缩放；到最小/最大尺寸时按边界夹取 */
void Window::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_locking) {
        relayout();
        return;
    }

    m_locking = true;
    const int w0 = width(), h0 = height();

    /* 用 1px 死区判断"哪个方向被拖动"：
       同一个方向没变（±1px 视为没变）就用另一个方向反解缩放比，
       两个方向都变了（拖角，或被系统夹取）就取较小的比例——
       这样无论拖左边、上边还是角，内容都严格等比。
       必须用"目标尺寸"反解 k，不能用 w/w_last 的比值：
       窗口尺寸被 WM 夹取 1px 时比值会 >1，一路乘上去会被钳到最大尺寸。 */
    const bool sameW = qAbs(w0 - m_last.width())  <= 1;
    const bool sameH = qAbs(h0 - m_last.height()) <= 1;

    double k;
    if (sameW && !sameH)      k = h0 / double(DESIGN_H);   // 只拖上/下边
    else if (sameH && !sameW) k = w0 / double(DESIGN_W);   // 只拖左/右边
    else                      k = qMin(w0 / double(DESIGN_W), h0 / double(DESIGN_H));

    k = qBound(double(minScale()), k, 1.0);

    const int tw = qBound(minimumWidth(),  qRound(DESIGN_W * k), maximumWidth());
    const int th = qBound(minimumHeight(), qRound(DESIGN_H * k), maximumHeight());

    if (tw != w0 || th != h0) {
        resize(tw, th);
        m_last = QSize(width(), height());
    } else {
        m_last = QSize(w0, h0);
    }
    m_locking = false;

    relayout();
    applyChrome();
}

/* Windows 默认把窗口限制在 SM_CXMINTRACK（约 112px）以上，
   这里接管 WM_GETMINMAXINFO，把最小跟踪尺寸降到棋盘 80×80 对应的 87×105 */
bool Window::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == QByteArrayLiteral("windows_generic_MSG")) {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_GETMINMAXINFO) {
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            const QMargins fm = windowHandle() ? windowHandle()->frameMargins() : QMargins();
            const qreal dpr = windowHandle() ? windowHandle()->devicePixelRatio() : 1.0;
            mmi->ptMinTrackSize.x = qRound(minimumWidth() * dpr) + fm.left() + fm.right();
            mmi->ptMinTrackSize.y = qRound(minimumHeight() * dpr) + fm.top() + fm.bottom();
            *result = 0;
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}

/* 计时器胶囊：点一下进暂停界面 */
bool Window::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_pillTime) {
        if (e->type() == QEvent::MouseButtonRelease) {
            if (static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton) {
                pauseGame(true);
                return true;
            }
        } else if (e->type() == QEvent::MouseButtonPress
                   || e->type() == QEvent::MouseButtonDblClick) {
            return true;                 // 吃掉按下，避免连带触发窗口拖动
        }
    }
    return QWidget::eventFilter(obj, e);
}

/*
 * 离开窗口 → 自动暂停计时；回到窗口 → 自动继续（与安卓一致）：
 *   - 最小化 / 还原
 *   - 窗口失去激活（切到别的程序 / 锁屏都会走这里）
 * m_wasActive 用来放过启动瞬间那一次"还没激活"，否则一开就是暂停态。
 * m_pauseShowing 期间（暂停卡片开着）主窗口必然失活，那不算"人离开"。
 */
void Window::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            pauseGame(false);
        } else if (m_autoPaused) {
            m_autoPaused = false;          // 还原回来 → 接着计时
            resumeGame();
        }
    } else if (e->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            m_wasActive = true;
            if (m_autoPaused) {            // 从后台回来 → 自动继续
                m_autoPaused = false;
                resumeGame();
            }
        } else if (m_wasActive && !m_pauseShowing) {
            pauseGame(false);
        }
    }
}

void Window::refresh()
{
    Game &g = m_game;
    m_pillTime->setText(QString("%1:%2")
                            .arg(g.seconds / 60, 2, 10, QLatin1Char('0'))
                            .arg(g.seconds % 60, 2, 10, QLatin1Char('0')));
    // 暂停时胶囊换成三角 + accent 色：既是状态提示，也是"点这里继续"的入口
    m_pillTime->setIcon(g.paused ? "play" : "clock");
    m_pillTime->setHot(g.paused);
    m_pillMiss->setText(QString::number(g.mistakes));
    m_pillMiss->setBad(g.mistakes > 0);
    m_diff->setValue(g.diff);
    m_bNotes->setOn(g.notesMode);
    m_bUndo->setEnabled(!g.history.isEmpty());
    m_bHint->setEnabled(!g.done && g.hints > 0);
    m_bHint->setToolTip(QStringLiteral("提示（剩 %1）").arg(g.hints));

    const QVector<int> rem = g.remaining();
    for (KeyBtn *k : m_keys)
        k->setCount(rem[k->digit()]);
    m_board->update();
}

void Window::place(int d)
{
    if (m_game.paused) return;
    m_game.place(d);
    afterChange();
}

void Window::eraseCell()
{
    if (m_game.paused) return;
    m_game.erase();
    afterChange();
}

void Window::undoMove()
{
    if (m_game.paused) return;
    if (m_game.undo()) afterChange();
}

/* 仅用于「笔记」功能的讲解截图：造两个带候选数的格子，并让笔记模式处于开启状态 */
void Window::demoNotes()
{
    Game &g = m_game;
    QVector<int> empties;
    for (int i = 0; i < NCELLS; ++i)
        if (!g.puzzle[i] && !g.values[i]) empties << i;
    if (empties.size() < 2) return;

    const int a = empties.at(empties.size() / 2);
    const int b = empties.at(qMin(empties.size() - 1, empties.size() / 2 + 4));
    g.sel = a;
    g.notes[a] = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8);  // 1 3 5 7 9
    g.notes[b] = (1 << 1) | (1 << 3) | (1 << 7);                        // 2 4 8
    g.notesMode = true;
    if (qEnvironmentVariableIsSet("SUDOKU_DUMP")) {
        QFile f(QCoreApplication::applicationDirPath() + "/notesidx.txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(QString("a=%1 b=%2 sel=%3\n").arg(a).arg(b).arg(g.sel).toUtf8());
    }
    refresh();
}

void Window::toggleNotes()
{
    if (m_game.paused) return;
    m_game.notesMode = !m_game.notesMode;
    refresh();
}

void Window::hintCell()
{
    if (m_game.paused) return;
    if (m_game.hint()) afterChange();
}

void Window::afterChange()
{
    refresh();
    if (!m_game.done && m_game.solved()) {
        m_game.done = true;
        m_game.running = false;
        refresh();
        showWin();
    }
    saveState();
}

void Window::onDiffChanged(const QString &k)
{
    newGame(k);
}

void Window::onWinMin()
{
    showMinimized();
}

/* 复位：回到设计默认尺寸（432×548，棋盘 400），并保证整窗落在屏幕内 */
void Window::onWinReset()
{
    QRect g = geometry();
    g.setSize(QSize(qRound(DESIGN_W), qRound(DESIGN_H)));
    if (QScreen *scr = screen()) {
        const QRect a = scr->availableGeometry();
        if (g.right() > a.right())   g.moveRight(a.right());
        if (g.bottom() > a.bottom()) g.moveBottom(a.bottom());
        if (g.left() < a.left())     g.moveLeft(a.left());
        if (g.top() < a.top())       g.moveTop(a.top());
    }
    m_locking = true;
    setGeometry(g);
    m_locking = false;
    m_last = size();
    relayout();
    applyChrome();
    m_settings.setValue(QStringLiteral("ui/geometry"), geometry());
}

void Window::onWinClose()
{
    close();
}

void Window::newGame(const QString &diff)
{
    const QString d = diff.isEmpty() ? m_game.diff : diff;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_game.newGame(d);
    QApplication::restoreOverrideCursor();
    refresh();
    saveState();
}

void Window::askNewGame()
{
    bool started = false;
    for (int i = 0; i < NCELLS; ++i)
        if (m_game.values[i] != m_game.puzzle[i]) { started = true; break; }

    if (!started) {
        newGame();
        return;
    }
    const QString c = cardDialog(this, QStringLiteral("开始新局？"),
                                 QStringLiteral("当前进度会被清掉。"), {},
                                 QStringLiteral("开始新局"), QStringLiteral("继续这局"));
    if (c == "primary") newGame();
}

void Window::showWin()
{
    const QString t = QString("%1:%2")
                          .arg(m_game.seconds / 60, 2, 10, QLatin1Char('0'))
                          .arg(m_game.seconds % 60, 2, 10, QLatin1Char('0'));
    const QString c = cardDialog(this, QStringLiteral("完成"),
                                 QStringLiteral("全部填对，漂亮。"),
                                 {{QStringLiteral("用时"), t},
                                  {QStringLiteral("错误"), QString::number(m_game.mistakes)}},
                                 QStringLiteral("再来一局"), QStringLiteral("看看棋盘"));
    if (c == "primary") newGame();
}

void Window::openThemeMenu()
{
    QMenu m(this);
    m.setStyleSheet(menuQss());
    const QString cur = m_settings.value("theme", "classic").toString();
    for (const ThemeDef &t : themes()) {
        QAction *a = m.addAction(t.name);
        a->setCheckable(true);
        a->setChecked(t.id == cur);
        connect(a, &QAction::triggered, this, [this, id = t.id] {
            m_settings.setValue("theme", id);
            applyTheme();
            refresh();
        });
    }
    m.addSeparator();
    const QString scheme = m_settings.value("scheme", "dark").toString();
    for (const QString &s : {QString("dark"), QStringLiteral("light")}) {
        QAction *a = m.addAction(s == "dark" ? QStringLiteral("深色") : QStringLiteral("浅色"));
        a->setCheckable(true);
        a->setChecked(s == scheme);
        connect(a, &QAction::triggered, this, [this, s] {
            m_settings.setValue("scheme", s);
            applyTheme();
            refresh();
        });
    }
    m.exec(m_bTheme->mapToGlobal(QPoint(-150, m_bTheme->height() + 6)));
}

void Window::applyTheme()
{
    const QString tid = m_settings.value("theme", "classic").toString();
    const QString sid = m_settings.value("scheme", "dark").toString();
    const ThemeDef *def = &themes().first();
    for (const ThemeDef &t : themes())
        if (t.id == tid) def = &t;
    pal() = (sid == "light") ? def->light : def->dark;
    update();
}

void Window::tick()
{
    if (!m_game.running || m_game.done || m_game.paused) return;
    ++m_game.seconds;
    m_pillTime->setText(QString("%1:%2")
                            .arg(m_game.seconds / 60, 2, 10, QLatin1Char('0'))
                            .arg(m_game.seconds % 60, 2, 10, QLatin1Char('0')));
    if (m_game.seconds % 10 == 0) saveState();
}

QString Window::timeText() const
{
    return QString("%1:%2")
        .arg(m_game.seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(m_game.seconds % 60, 2, 10, QLatin1Char('0'));
}

/* 置暂停位。showNow=true 立刻弹暂停界面（点计时器）；false 只静默暂停（离开窗口）。 */
void Window::pauseGame(bool showNow)
{
    if (m_game.done) return;
    if (m_closing || !isVisible()) return;    // 正在关闭/已隐藏：不是"人离开"，别再写一次暂停态
    if (showNow) {
        m_autoPaused = false;                 // 用户主动暂停：必须点三角才继续
    } else if (!m_game.paused) {
        m_autoPaused = true;                  // 离开窗口触发的暂停：回到窗口自动继续
    }
    if (!m_game.paused) {
        m_game.paused = true;
        refresh();
        saveState();
    }
    if (showNow) showPause();
}

void Window::resumeGame()
{
    m_autoPaused = false;
    if (!m_game.paused) return;
    m_game.paused = false;
    refresh();
    saveState();
}

void Window::showPause()
{
    if (!m_game.paused || m_pauseShowing) return;
    m_pauseShowing = true;
    refresh();
    if (pauseDialog(this, timeText())) resumeGame();   // 只有点三角才返回 true
    m_pauseShowing = false;
}

static QString joinInts(const QVector<int> &v)
{
    QStringList l;
    l.reserve(v.size());
    for (int x : v) l << QString::number(x);
    return l.join(',');
}

QVector<int> Window::readArr(const QString &key, const QVector<int> &def)
{
    const QString raw = m_settings.value(key, "").toString();
    if (raw.isEmpty()) return def;
    const QStringList parts = raw.split(',');
    if (parts.size() != NCELLS) return def;
    QVector<int> out;
    out.reserve(NCELLS);
    for (const QString &s : parts) out << s.toInt();
    return out;
}

void Window::saveState()
{
    Game &g = m_game;
    m_settings.setValue("board/puzzle", joinInts(g.puzzle));
    m_settings.setValue("board/solution", joinInts(g.solution));
    m_settings.setValue("board/values", joinInts(g.values));
    m_settings.setValue("board/notes", joinInts(g.notes));
    m_settings.setValue("board/hinted", joinInts(g.hinted));
    m_settings.setValue("board/diff", g.diff);
    m_settings.setValue("board/mistakes", g.mistakes);
    m_settings.setValue("board/seconds", g.seconds);
    m_settings.setValue("board/hints", g.hints);
    m_settings.setValue("board/done", g.done ? 1 : 0);
    m_settings.setValue("board/paused", g.paused ? 1 : 0);
    m_settings.sync();
}

void Window::loadState()
{
    const QVector<int> puzzle = readArr("board/puzzle", {});
    const QVector<int> solution = readArr("board/solution", {});
    if (puzzle.isEmpty() || solution.isEmpty()) {
        newGame("medium");
        return;
    }
    Game &g = m_game;
    g.puzzle = puzzle;
    g.solution = solution;
    g.values = readArr("board/values", puzzle);
    g.notes = readArr("board/notes", QVector<int>(NCELLS, 0));
    g.hinted = readArr("board/hinted", QVector<int>(NCELLS, 0));
    g.diff = m_settings.value("board/diff", "medium").toString();
    g.mistakes = m_settings.value("board/mistakes", 0).toInt();
    g.seconds = m_settings.value("board/seconds", 0).toInt();
    g.hints = m_settings.value("board/hints", 3).toInt();
    g.done = m_settings.value("board/done", 0).toInt() != 0;
    // 冷启动不继承暂停态：暂停表示"人暂时离开了窗口"，
    // 重新启动程序说明人已回到桌前，一上来就弹暂停层会像是卡死了。
    // （board/paused 仍然写盘，只用于异常退出后排查，不再回读。）
    g.paused = false;
    g.sel = -1;
    g.running = !g.done;
}

void Window::keyPressEvent(QKeyEvent *e)
{
    if (m_game.paused) {                 // 暂停时键盘也不许动棋局
        QWidget::keyPressEvent(e);
        return;
    }
    const int k = e->key();
    if (k >= Qt::Key_1 && k <= Qt::Key_9) { place(k - Qt::Key_0); return; }
    if (k == Qt::Key_Backspace || k == Qt::Key_Delete || k == Qt::Key_0) { eraseCell(); return; }
    if (k == Qt::Key_N) { toggleNotes(); return; }
    if (k == Qt::Key_H) { hintCell(); return; }
    if (k == Qt::Key_Z && (e->modifiers() & Qt::ControlModifier)) { undoMove(); return; }

    int d = 0;
    if (k == Qt::Key_Up) d = -9;
    else if (k == Qt::Key_Down) d = 9;
    else if (k == Qt::Key_Left) d = -1;
    else if (k == Qt::Key_Right) d = 1;
    if (d != 0) {
        if (m_game.sel < 0) {
            m_game.sel = 0;
        } else {
            int n = m_game.sel + d;
            if (k == Qt::Key_Left && m_game.sel % 9 == 0) n = m_game.sel;
            else if (k == Qt::Key_Right && m_game.sel % 9 == 8) n = m_game.sel;
            if (n >= 0 && n < NCELLS) m_game.sel = n;
        }
        refresh();
        return;
    }
    QWidget::keyPressEvent(e);
}

void Window::closeEvent(QCloseEvent *e)
{
    m_closing = true;        // 先立标志，避免关闭过程中的失活事件把 paused 又置回 1
    saveState();
    QWidget::closeEvent(e);
}

void Window::demoState()
{
    Game &g = m_game;
    int sel = -1;
    for (int i = 0; i < NCELLS; ++i)
        if (!g.puzzle[i] && !g.values[i]) { sel = i; break; }
    if (sel < 0) return;
    g.sel = sel;

    int filled = 0;
    for (int i = 0; i < NCELLS && filled < 2; ++i)
        if (!g.puzzle[i] && !g.values[i] && i != sel) { g.values[i] = g.solution[i]; ++filled; }

    for (int i = 0; i < NCELLS; ++i)
        if (!g.puzzle[i] && !g.values[i] && i != sel) {
            int v = g.solution[i] % 9 + 1;
            if (v == g.solution[i]) v = (g.solution[i] + 1) % 9 + 1;
            g.values[i] = v;
            break;
        }

    for (int i = 0; i < NCELLS; ++i)
        if (!g.puzzle[i] && !g.values[i] && i != sel) { g.notes[i] = 0x56; break; }

    const int r0 = sel / 9;
    int dup = 0;
    for (int k = 0; k < 9; ++k)
        if (g.values[r0 * 9 + k]) { dup = g.values[r0 * 9 + k]; break; }
    if (dup) { g.values[sel] = dup; g.notes[sel] = 0; }
    g.mistakes = 2;
    refresh();
}
