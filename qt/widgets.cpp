#include "widgets.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QMouseEvent>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QApplication>
#include <QPushButton>
#include <QtMath>

/* ------------------------------------------------------------------ 字体 */
QFont uiFont(qreal pt, QFont::Weight w)
{
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSizeF(qMax(0.5, pt));
    f.setWeight(w);
    return f;
}

QFont digitFont(qreal pt, QFont::Weight w)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPointSizeF(qMax(0.5, pt));
    f.setWeight(w);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    f.setFeature(QFont::Tag("tnum"), 1);
#endif
    return f;
}

/* ------------------------------------------------------------------ 图标 */
void paintIcon(QPainter &p, const QString &name, const QColor &c, qreal box)
{
    p.save();
    const qreal lw = qMax(0.55, 1.7 * box / 24.0);   // 小尺寸下限宽，避免图标消失
    p.setPen(QPen(c, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const qreal s = box / 24.0;
    p.scale(s, s);

    if (name == "clock") {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawLine(QPointF(12, 7.5), QPointF(12, 12));
        p.drawLine(QPointF(12, 12), QPointF(15, 13.8));
    } else if (name == "cross") {
        p.drawLine(QPointF(6, 6), QPointF(18, 18));
        p.drawLine(QPointF(18, 6), QPointF(6, 18));
    } else if (name == "undo") {
        QPainterPath path;
        path.moveTo(3, 8);
        path.lineTo(3, 13);
        path.lineTo(8, 13);
        p.drawPath(path);
        QPainterPath arc;
        arc.moveTo(5.5, 11);
        arc.cubicTo(8, 6.2, 13.5, 5.2, 16.8, 8.4);
        arc.cubicTo(19.6, 11.2, 18.6, 15.4, 15, 17);
        p.drawPath(arc);
    } else if (name == "pencil") {
        p.drawLine(QPointF(12, 20.4), QPointF(21, 20.4));
        QPainterPath path;
        path.moveTo(16.6, 3.6);
        path.lineTo(20.4, 7.4);
        path.lineTo(7, 20.8);
        path.lineTo(3, 21.0);
        path.lineTo(3.2, 17.0);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(14.2, 6.0), QPointF(18.0, 9.8));
    } else if (name == "eraser") {
        p.drawLine(QPointF(8, 20.4), QPointF(20, 20.4));
        QPainterPath path;
        path.moveTo(4.6, 15.4);
        path.lineTo(11.4, 8.6);
        path.cubicTo(12.2, 7.8, 13.4, 7.8, 14.2, 8.6);
        path.lineTo(17.6, 12.0);
        path.cubicTo(18.4, 12.8, 18.4, 14.0, 17.6, 14.8);
        path.lineTo(12.8, 19.2);
        path.lineTo(7.3, 19.2);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(8.2, 11.8), QPointF(13.8, 17.4));
    } else if (name == "bulb") {
        p.drawLine(QPointF(9.2, 18.0), QPointF(14.8, 18.0));
        p.drawLine(QPointF(10.3, 21.0), QPointF(13.7, 21.0));
        QPainterPath path;
        path.moveTo(12, 3);
        path.cubicTo(8.7, 3, 6.0, 5.7, 6.0, 9.0);
        path.cubicTo(6.0, 11.0, 7.0, 12.2, 8.0, 13.4);
        path.cubicTo(8.7, 14.3, 9.0, 15.0, 9.0, 16.0);
        path.lineTo(15.0, 16.0);
        path.cubicTo(15.0, 15.0, 15.3, 14.3, 16.0, 13.4);
        path.cubicTo(17.0, 12.2, 18.0, 11.0, 18.0, 9.0);
        path.cubicTo(18.0, 5.7, 15.3, 3, 12, 3);
        p.drawPath(path);
    } else if (name == "plus") {
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (name == "palette") {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.setBrush(c);
        for (const QPointF &pt : {QPointF(9.2, 9.2), QPointF(14.8, 9.2), QPointF(9.6, 14.6)})
            p.drawEllipse(pt, 1.15, 1.15);
    } else if (name == "play") {
        // 实心三角。取 6.8~17.2，包围盒中心正好在 24 格中线 12 上，
        // 放进圆形按钮后三角中心即圆心，无需再偏移
        QPainterPath t;
        t.moveTo(6.8, 5.0);
        t.lineTo(17.2, 12.0);
        t.lineTo(6.8, 19.0);
        t.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(t);
        p.setBrush(Qt::NoBrush);
    } else if (name == "chevron") {
        p.drawLine(QPointF(6.5, 9.5), QPointF(12, 15));
        p.drawLine(QPointF(12, 15), QPointF(17.5, 9.5));
    }
    p.restore();
}

QPixmap makeIcon(const QString &name, const QColor &c, qreal box, qreal dpr)
{
    const int px = qMax(2, int(qCeil(box * dpr)));
    QPixmap pm(px, px);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintIcon(p, name, c, box);
    p.end();
    return pm;
}

QString menuQss()
{
    return QString(
               "QMenu{background:%1;border:1px solid %2;padding:5px;color:%3;"
               "font-family:'Microsoft YaHei UI';font-size:12px;}"
               "QMenu::item{padding:6px 20px 6px 24px;border-radius:6px;}"
               "QMenu::item:selected{background:%4;}"
               "QMenu::item:checked{color:%5;}"
               "QMenu::separator{height:1px;background:%6;margin:5px 6px;}")
        .arg(pal().bg.name(), hexa(pal().text, 0.14), pal().text.name(),
             pal().surface.name(), pal().entry.name(), hexa(pal().text, 0.12));
}

/* ------------------------------------------------------------------ Pill */
Pill::Pill(const QString &icon, const QString &text, QWidget *parent)
    : QWidget(parent), m_icon(icon), m_text(text)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyScale(1.0);
}

void Pill::applyScale(qreal k)
{
    m_k = k;
    setFont(uiFont(9.0 * k, QFont::DemiBold));
    setFixedSize(sizeHint());
    updateGeometry();
    update();
}

void Pill::setText(const QString &t)
{
    if (t == m_text) return;
    m_text = t;
    setFixedSize(sizeHint());   // 文字变长（如 0:00 -> 12:34）时同步宽度
    updateGeometry();
    update();
}

void Pill::setBad(bool bad)
{
    if (bad == m_bad) return;
    m_bad = bad;
    update();
}

void Pill::setIcon(const QString &name)
{
    if (name == m_icon) return;
    m_icon = name;        // 图标非空/非空之间切换，宽度不变，无需重算 sizeHint
    update();
}

void Pill::setHot(bool hot)
{
    if (hot == m_hot) return;
    m_hot = hot;
    update();
}

QSize Pill::sizeHint() const
{
    const QFontMetricsF fm(uiFont(9.0 * m_k, QFont::DemiBold));
    qreal w = fm.horizontalAdvance(m_text) + 22 * m_k;
    if (!m_icon.isEmpty()) w += 18 * m_k;
    return QSize(qCeil(qMax(w, 10.0)), qMax(4, int(30 * m_k)));
}

void Pill::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal k = m_k;
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 9 * k, 9 * k);
    p.fillPath(path, pal().surface);

    qreal x = 9 * k;
    const qreal cy = height() / 2.0;
    const QColor ic = m_bad ? pal().mistake : (m_hot ? pal().accent : pal().hint);
    if (!m_icon.isEmpty()) {
        const qreal ib = 13 * k;
        p.drawPixmap(QPointF(x, cy - ib / 2.0), makeIcon(m_icon, ic, ib));
        x += ib + 5 * k;
    }
    p.setFont(uiFont(9.0 * k, QFont::DemiBold));
    p.setPen(m_bad ? pal().mistake : (m_hot ? pal().accent : pal().text));
    p.drawText(QRectF(x - 1, 0, width() - x + 1, height()),
               Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

/* ------------------------------------------------------------------ IconBtn */
IconBtn::IconBtn(const QString &name, const QString &tip, QWidget *parent)
    : QAbstractButton(parent), m_name(name)
{
    setToolTip(tip);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyScale(1.0);
}

void IconBtn::applyScale(qreal k)
{
    m_k = k;
    const int px = qMax(4, int(28 * k));
    setFixedSize(px, px);
    update();
}

/* ---------------- CapBtn：窗口控制按钮 ---------------- */

CapBtn::CapBtn(const QString &name, const QString &tip, bool danger, QWidget *parent)
    : QAbstractButton(parent), m_name(name), m_danger(danger)
{
    setToolTip(tip);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyScale(1.0);
}

void CapBtn::applyScale(qreal k)
{
    m_k = k;
    const int px = qMax(5, int(22 * k));
    setFixedSize(px, px);
    update();
}

void CapBtn::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void CapBtn::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void CapBtn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal k = m_k;
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    if (m_hover) {
        QPainterPath path;
        path.addRoundedRect(r, qMax(2.0, 6 * k), qMax(2.0, 6 * k));
        QColor hb = m_danger ? col(pal().mistake, isDown() ? 0.60 : 0.30)
                             : (isDown() ? pal().surface.darker(118) : pal().surface);
        p.fillPath(path, hb);
    }

    QColor c;
    if (m_danger) c = m_hover ? pal().mistake : col(pal().mistake, 0.85);
    else          c = m_hover ? pal().text : col(pal().hint, 0.95);

    /* 直接矢量绘制：13px 级的小图标走 makeIcon 会被半透明下采样吃掉描边 */
    const qreal lw = qMax(1.0, 1.35 * k);
    p.setPen(QPen(c, lw, Qt::SolidLine, Qt::RoundCap));
    const qreal cx = width() / 2.0, cy = height() / 2.0;
    const qreal a = qMax(2.6, 4.2 * k);      // 到中心的半边长
    const qreal b = a * 0.62;                // 括号短臂

    if (m_name == QLatin1String("winmin")) {
        p.drawLine(QPointF(cx - a, cy), QPointF(cx + a, cy));
    } else if (m_name == QLatin1String("winclose")) {
        const qreal d = a * 0.8;
        p.drawLine(QPointF(cx - d, cy - d), QPointF(cx + d, cy + d));
        p.drawLine(QPointF(cx + d, cy - d), QPointF(cx - d, cy + d));
    } else {                                  // winreset：四角取景框
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                p.drawLine(QPointF(cx + sx * a, cy + sy * (a - b)),
                           QPointF(cx + sx * a, cy + sy * a));
                p.drawLine(QPointF(cx + sx * (a - b), cy + sy * a),
                           QPointF(cx + sx * a, cy + sy * a));
            }
        }
    }
}

void IconBtn::setOn(bool on)
{
    if (on == m_on) return;
    m_on = on;
    update();
}

void IconBtn::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void IconBtn::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void IconBtn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal k = m_k;
    QColor bg;
    bool hasBg = false;
    if (m_on) {
        bg = mixc(pal().accent, pal().bg, pal().same);
        hasBg = true;
    } else if (m_hover && isEnabled()) {
        bg = pal().surface;
        hasBg = true;
    }
    if (hasBg) {
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8 * k, 8 * k);
        p.fillPath(path, bg);
    }
    QColor c = m_on ? pal().entry : (m_hover ? pal().text : pal().hint);
    if (!isEnabled()) {
        c = col(pal().hint, 0.3);
        p.setOpacity(0.45);
    }
    const qreal ib = width() * 0.61;                 // ≈17px @ k=1
    const qreal dpr = qBound(2.0, 12.0 / qMax(1.0, ib), 4.0);
    p.drawPixmap(QPointF((width() - ib) / 2.0, (height() - ib) / 2.0),
                 makeIcon(m_name, c, ib, dpr));
}

/* ------------------------------------------------------------------ KeyBtn */
KeyBtn::KeyBtn(int digit, QWidget *parent)
    : QAbstractButton(parent), m_digit(digit)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);
    applyScale(1.0);
}

void KeyBtn::applyScale(qreal k)
{
    m_k = k;
    setFixedHeight(qMax(4, int(42 * k)));
    update();
}

void KeyBtn::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void KeyBtn::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void KeyBtn::mousePressEvent(QMouseEvent *e)
{
    m_down = true;
    QAbstractButton::mousePressEvent(e);
    update();
}

void KeyBtn::mouseReleaseEvent(QMouseEvent *e)
{
    m_down = false;
    QAbstractButton::mouseReleaseEvent(e);
    update();
}

void KeyBtn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal k = m_k;
    const bool done = m_count <= 0;
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10 * k, 10 * k);
    QColor bg = pal().surface;
    if (!done && m_down) bg = mixc(pal().accent, pal().bg, pal().selected - 2);
    else if (!done && m_hover) bg = mixc(pal().accent, pal().bg, pal().same - 4);
    p.fillPath(path, bg);

    p.setOpacity(done ? 0.28 : 1.0);
    p.setFont(digitFont(14.6 * k, QFont::DemiBold));
    p.setPen(done ? pal().hint : pal().entry);
    p.drawText(rect(), Qt::AlignCenter, QString::number(m_digit));

    if (m_count > 0 && m_count < 9) {
        p.setFont(digitFont(6.2 * k, QFont::Medium));
        p.setPen(col(pal().hint, 0.85));
        p.drawText(QRectF(width() - 15 * k, 4 * k, 11 * k, 11 * k),
                   Qt::AlignRight | Qt::AlignTop, QString::number(m_count));
    }
    p.setOpacity(1.0);
}

/* ------------------------------------------------------------------ DiffBtn */
DiffBtn::DiffBtn(QWidget *parent) : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyScale(1.0);
}

void DiffBtn::applyScale(qreal k)
{
    m_k = k;
    setFixedSize(qMax(12, int(70 * k)), qMax(4, int(30 * k)));
    setFont(uiFont(8.0 * k, QFont::DemiBold));
    update();
}

QSize DiffBtn::sizeHint() const
{
    return QSize(qMax(12, int(70 * m_k)), qMax(4, int(30 * m_k)));
}

void DiffBtn::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void DiffBtn::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void DiffBtn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal k = m_k;
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 9 * k, 9 * k);
    p.fillPath(path, m_hover ? mixc(pal().accent, pal().surface, 20) : pal().surface);

    static const QMap<QString, QString> cn{
        {"easy", "简单"}, {"medium", "中等"}, {"hard", "困难"}, {"expert", "专家"}};
    p.setFont(uiFont(8.0 * k, QFont::DemiBold));
    p.setPen(pal().text);
    p.drawText(QRectF(10 * k, 0, width() - 26 * k, height()),
               Qt::AlignVCenter | Qt::AlignLeft, cn.value(m_value, "中等"));
    const qreal cb = 11 * k;
    p.drawPixmap(QPointF(width() - 19 * k, height() / 2.0 - cb / 2.0),
                 makeIcon("chevron", pal().hint, cb));
}

void DiffBtn::mousePressEvent(QMouseEvent *)
{
    QMenu m(this);
    m.setStyleSheet(menuQss());
    static const QMap<QString, QString> cn{
        {"easy", "简单"}, {"medium", "中等"}, {"hard", "困难"}, {"expert", "专家"}};
    static const QStringList opts{"easy", "medium", "hard", "expert"};
    for (const QString &k : opts) {
        QAction *a = m.addAction(cn.value(k));
        a->setCheckable(true);
        a->setChecked(k == m_value);
        connect(a, &QAction::triggered, this, [this, k] { pick(k); });
    }
    m.exec(mapToGlobal(QPoint(width() - 70, height() + 6)));
}

void DiffBtn::pick(const QString &k)
{
    m_value = k;
    update();
    if (onChange) onChange(k);
    emit changed(k);
}

/* ------------------------------------------------------------------ Board */
Board::Board(Game *g, QWidget *parent) : QWidget(parent), m_g(g)
{
    setCursor(Qt::PointingHandCursor);
    applyScale(1.0);
}

void Board::applyScale(qreal k)
{
    m_k = k;
    const int px = qMax(int(MIN_BOARD), qRound(BOARD_PX * k));
    setFixedSize(px, px);
    update();
}

void Board::mousePressEvent(QMouseEvent *e)
{
    const qreal border = qMax(1.0, 2.0 * m_k);
    const qreal cw = (width() - 2 * border) / 9.0;
    const qreal ch = (height() - 2 * border) / 9.0;
    const qreal x = e->position().x() - border;
    const qreal y = e->position().y() - border;
    if (x >= 0 && x < 9 * cw && y >= 0 && y < 9 * ch) {
        const int i = int(y / ch) * 9 + int(x / cw);
        m_g->sel = (m_g->sel == i) ? -1 : i;
        update();
        if (onChanged) onChanged();
    }
}

void Board::paintEvent(QPaintEvent *)
{
    Game &g = *m_g;
    const qreal k = m_k;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QSet<int> bad = g.conflicts();
    const int si = g.sel;
    const int sv = (si >= 0) ? g.values[si] : 0;
    const int sr = (si >= 0) ? si / 9 : -1;
    const int sc = (si >= 0) ? si % 9 : -1;
    const int sbr = (sr >= 0) ? sr - sr % 3 : -9;
    const int sbc = (sc >= 0) ? sc - sc % 3 : -9;

    const qreal border = qMax(1.0, 2.0 * k);
    const qreal radius = qMax(2.0, 8.0 * k);
    const qreal cw = (width() - 2 * border) / 9.0;
    const qreal ch = (height() - 2 * border) / 9.0;

    const QRectF outer(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath clip;
    clip.addRoundedRect(outer, radius, radius);
    p.setClipPath(clip);
    p.fillRect(rect(), pal().bg);

    const QColor cArea = mixc(pal().accent, pal().bg, pal().area);
    const QColor cSame = mixc(pal().accent, pal().bg, pal().same);
    const QColor cSel = mixc(pal().accent, pal().bg, pal().selected);
    const QColor cDup = mixc(pal().mistake, pal().bg, pal().conflict);

    for (int i = 0; i < NCELLS; ++i) {
        const int r = i / 9, c = i % 9;
        const qreal x = border + c * cw;
        const qreal y = border + r * ch;
        const QRectF cellRect(x, y, cw, ch);
        const int v = g.values[i];
        const bool isSel = (i == si);
        const bool isPeer = (si >= 0) &&
                            (r == sr || c == sc ||
                             (r >= sbr && r < sbr + 3 && c >= sbc && c < sbc + 3));

        bool has = false;
        QColor bg;
        if (sv) {
            if (v == sv) { bg = cSame; has = true; }
            else if (!v && isPeer) { bg = cArea; has = true; }
        } else if (isPeer && !isSel) {
            bg = cArea; has = true;
        }
        if (bad.contains(i)) { bg = cDup; has = true; }
        if (isSel) { bg = cSel; has = true; }
        if (has) p.fillRect(cellRect, bg);

        if (v) {
            QColor fg;
            QFont::Weight w;
            if (g.puzzle[i]) { fg = pal().text; w = QFont::DemiBold; }
            else if (v == g.solution[i]) { fg = pal().entry; w = QFont::Medium; }
            else { fg = pal().mistake; w = QFont::Medium; }
            if (g.hinted[i]) fg = pal().entry;
            p.setFont(digitFont(20.2 * k, w));
            p.setPen(fg);
            if (sv && v != sv && !isSel) p.setOpacity(0.40);
            p.drawText(cellRect, Qt::AlignCenter, QString::number(v));
            p.setOpacity(1.0);
        } else if (g.notes[i]) {
            p.setFont(digitFont(7.4 * k, QFont::Medium));
            p.setPen(pal().hint);
            for (int d = 1; d <= 9; ++d) {
                if (!(g.notes[i] & (1 << (d - 1)))) continue;
                const qreal nx = x + ((d - 1) % 3) * cw / 3.0;
                const qreal ny = y + ((d - 1) / 3) * ch / 3.0;
                p.drawText(QRectF(nx, ny, cw / 3.0, ch / 3.0), Qt::AlignCenter, QString::number(d));
            }
        }
    }

    // 网格线（小尺寸下限宽）
    const qreal wThin = qMax(0.7, 1.0 * k);
    const qreal wRule = qMax(1.5, 2.0 * k);
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 1; i < 9; ++i) {
        const bool rule = (i % 3 == 0);
        p.setPen(QPen(col(pal().text, rule ? 0.62 : 0.18), rule ? wRule : wThin));
        const qreal px = border + i * cw;
        const qreal py = border + i * ch;
        p.drawLine(QPointF(px, border), QPointF(px, height() - border));
        p.drawLine(QPointF(border, py), QPointF(width() - border, py));
    }

    // 选中描边
    if (si >= 0) {
        const int r = si / 9, c = si % 9;
        const QRectF cellRect(border + c * cw, border + r * ch, cw, ch);
        const qreal inset = qMax(0.4, 0.8 * k);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(pal().accent, qMax(1.0, 1.6 * k)));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cellRect.adjusted(inset, inset, -inset, -inset),
                          qMax(0.8, 2.0 * k), qMax(0.8, 2.0 * k));
    }

    // 外框
    p.setClipping(false);
    p.setPen(QPen(col(pal().text, 0.62), border));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outer, radius, radius);
}

/* ------------------------------------------------------------------ Card（固定尺寸，不随窗口缩放） */
QString cardDialog(QWidget *parent, const QString &title, const QString &sub,
                   const QVector<QPair<QString, QString>> &stats,
                   const QString &primary, const QString &secondary)
{
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setModal(true);

    auto *wrap = new QVBoxLayout(&dlg);
    wrap->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QFrame;
    panel->setObjectName("panel");
    panel->setFixedWidth(302);
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 100));
    panel->setGraphicsEffect(shadow);
    wrap->addWidget(panel);

    auto *lay = new QVBoxLayout(panel);
    lay->setContentsMargins(20, 22, 20, 18);
    lay->setSpacing(0);

    auto *h = new QLabel(title);
    h->setFont(uiFont(13.5, QFont::DemiBold));
    h->setAlignment(Qt::AlignCenter);
    h->setStyleSheet("color:" + pal().text.name() + ";");
    lay->addWidget(h);

    if (!sub.isEmpty()) {
        auto *s = new QLabel(sub);
        s->setFont(uiFont(8.5));
        s->setAlignment(Qt::AlignCenter);
        s->setWordWrap(true);
        s->setStyleSheet("color:" + pal().hint.name() + ";");
        lay->addSpacing(6);
        lay->addWidget(s);
    }

    if (!stats.isEmpty()) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 14, 0, 14);
        row->setSpacing(26);
        row->addStretch(1);
        for (const auto &kv : stats) {
            auto *box = new QVBoxLayout;
            box->setSpacing(3);
            auto *val = new QLabel(kv.second);
            val->setFont(digitFont(13.5, QFont::DemiBold));
            val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet("color:" + pal().text.name() + ";");
            auto *cap = new QLabel(kv.first);
            cap->setFont(uiFont(7.5));
            cap->setAlignment(Qt::AlignCenter);
            cap->setStyleSheet("color:" + pal().hint.name() + ";");
            box->addWidget(val);
            box->addWidget(cap);
            row->addLayout(box);
        }
        row->addStretch(1);
        lay->addLayout(row);
    } else {
        lay->addSpacing(18);
    }

    QString choice;
    auto addBtn = [&](const QString &text, bool primaryStyle) {
        if (text.isEmpty()) return;
        auto *b = new QPushButton;
        b->setText(text);
        b->setFixedHeight(42);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(uiFont(9.0, QFont::DemiBold));
        if (primaryStyle) {
            b->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:0;border-radius:11px;}"
                                     "QPushButton:hover{background:%3;}")
                                 .arg(pal().accent.name(), pal().onAccent.name(),
                                      mixc(pal().accent, QColor(255, 255, 255), 86).name()));
        } else {
            b->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:0;border-radius:11px;}"
                                     "QPushButton:hover{background:%3;}")
                                 .arg(pal().surface.name(), pal().entry.name(),
                                      mixc(pal().entry, pal().surface, 16).name()));
        }
        const QString val = primaryStyle ? "primary" : "secondary";
        QObject::connect(b, &QPushButton::clicked, &dlg, [&dlg, &choice, val] {
            choice = val;
            dlg.accept();
        });
        lay->addWidget(b);
        lay->addSpacing(8);
    };
    addBtn(primary, true);
    addBtn(secondary, false);

    panel->setStyleSheet(QString("QFrame#panel{background:%1;border-radius:18px;}")
                             .arg(pal().bg.name()));
    dlg.adjustSize();
    if (parent) {
        const QPoint topLeft = parent->mapToGlobal(QPoint(0, 0));
        dlg.move(topLeft + QPoint((parent->width() - dlg.width()) / 2,
                                  (parent->height() - dlg.height()) / 2));
    }
    dlg.exec();
    return choice;
}

/* ------------------------------------------------------------------ Pause（暂停界面） */
bool pauseDialog(QWidget *parent, const QString &time)
{
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setModal(true);

    auto *wrap = new QVBoxLayout(&dlg);
    wrap->setContentsMargins(0, 0, 0, 0);

    auto *panel = new QFrame;
    panel->setObjectName("panel");
    panel->setFixedWidth(230);
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 120));
    panel->setGraphicsEffect(shadow);
    wrap->addWidget(panel);

    auto *lay = new QVBoxLayout(panel);
    lay->setContentsMargins(20, 24, 20, 24);
    lay->setSpacing(0);

    auto *cap = new QLabel(QStringLiteral("已暂停"));
    cap->setFont(uiFont(8.5, QFont::DemiBold));
    cap->setAlignment(Qt::AlignCenter);
    cap->setStyleSheet("color:" + pal().hint.name() + ";");
    lay->addWidget(cap);

    auto *t = new QLabel(time);
    t->setFont(digitFont(27, QFont::DemiBold));
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:" + pal().text.name() + ";");
    lay->addSpacing(6);
    lay->addWidget(t);

    // 圆形三角按钮：唯一的恢复入口
    auto *play = new QPushButton;
    play->setFixedSize(62, 62);
    play->setCursor(Qt::PointingHandCursor);
    play->setToolTip(QStringLiteral("继续"));
    play->setIcon(QIcon(makeIcon("play", pal().onAccent, 24)));
    play->setIconSize(QSize(24, 24));
    play->setStyleSheet(
        QString("QPushButton{background:%1;border:0;border-radius:31px;}"
                "QPushButton:hover{background:%2;}")
            .arg(pal().accent.name(),
                 mixc(pal().accent, QColor(255, 255, 255), 86).name()));

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 20, 0, 0);
    row->addStretch(1);
    row->addWidget(play);
    row->addStretch(1);
    lay->addLayout(row);

    bool resumed = false;
    QObject::connect(play, &QPushButton::clicked, &dlg, [&dlg, &resumed] {
        resumed = true;
        dlg.accept();
    });

    panel->setStyleSheet(QString("QFrame#panel{background:%1;border-radius:18px;}")
                             .arg(pal().bg.name()));
    dlg.adjustSize();
    if (parent) {
        const QPoint topLeft = parent->mapToGlobal(QPoint(0, 0));
        dlg.move(topLeft + QPoint((parent->width() - dlg.width()) / 2,
                                  (parent->height() - dlg.height()) / 2));
    }
    dlg.exec();
    return resumed;
}
