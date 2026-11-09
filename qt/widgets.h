#ifndef WIDGETS_H
#define WIDGETS_H

#include <QWidget>
#include <QAbstractButton>
#include <QDialog>
#include <QPair>
#include <QVector>
#include <functional>

#include "game.h"
#include "palette.h"

// 设计稿尺寸：棋盘 400，整窗 432×548（顶部 26 是窗口按钮条）。以此为 1.0 等比缩放。
static const qreal DESIGN_W = 432.0;
static const qreal DESIGN_H = 548.0;
static const qreal CAP_H = 26.0;     // 标题条高度（放最小化/复位/关闭）
static const qreal BAR_H = 30.0;     // 主工具栏高度
static const qreal BOARD_PX = 400.0;
/* 最小棋盘 80×80 → 整窗 86×104（k=0.2）。
   注意：带系统标题栏时 Windows 有硬地板（客户区宽 ≥120），
   那种模式下改用 FRAMED_MIN_SCALE（窗口 120×145）。 */
static const qreal MIN_BOARD = 80.0;
static const qreal MIN_SCALE = MIN_BOARD / BOARD_PX;        // 0.2
static const qreal FRAMED_MIN_SCALE = 120.0 / DESIGN_W;     // 0.27778

QFont uiFont(qreal pt, QFont::Weight w = QFont::Normal);
QFont digitFont(qreal pt, QFont::Weight w = QFont::Medium);
void paintIcon(QPainter &p, const QString &name, const QColor &c, qreal box);
QPixmap makeIcon(const QString &name, const QColor &c, qreal box, qreal dpr = 2.0);
QString menuQss();

/* ------------------------------------------------------------------ 状态胶囊 */
class Pill : public QWidget
{
    Q_OBJECT
public:
    explicit Pill(const QString &icon, const QString &text, QWidget *parent = nullptr);
    void setText(const QString &t);
    void setBad(bool bad);
    void setIcon(const QString &name);      // 暂停时换成三角
    void setHot(bool hot);                  // 暂停态：图标与文字用 accent 色
    void applyScale(qreal k);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_icon, m_text;
    bool m_bad = false;
    bool m_hot = false;
    qreal m_k = 1.0;
};

/* ------------------------------------------------------------------ 图标按钮 */
class IconBtn : public QAbstractButton
{
    Q_OBJECT
public:
    explicit IconBtn(const QString &name, const QString &tip = QString(), QWidget *parent = nullptr);
    void setOn(bool on);
    void applyScale(qreal k);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QString m_name;
    bool m_hover = false, m_on = false;
    qreal m_k = 1.0;
};

/* 窗口控制按钮（最小化 / 复位默认大小 / 关闭），画在卡片顶部窄条里 */
class CapBtn : public QAbstractButton
{
    Q_OBJECT
public:
    explicit CapBtn(const QString &name, const QString &tip = QString(),
                    bool danger = false, QWidget *parent = nullptr);
    void applyScale(qreal k);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QString m_name;
    bool m_danger = false;
    bool m_hover = false;
    qreal m_k = 1.0;
};

/* ------------------------------------------------------------------ 数字键 */
class KeyBtn : public QAbstractButton
{
    Q_OBJECT
public:
    explicit KeyBtn(int digit, QWidget *parent = nullptr);
    void setCount(int c) { if (c != m_count) { m_count = c; update(); } }
    int digit() const { return m_digit; }
    void applyScale(qreal k);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    int m_digit = 1;
    int m_count = 9;
    bool m_hover = false, m_down = false;
    qreal m_k = 1.0;
};

/* ------------------------------------------------------------------ 难度选择 */
class DiffBtn : public QWidget
{
    Q_OBJECT
public:
    explicit DiffBtn(QWidget *parent = nullptr);
    QString value() const { return m_value; }
    void setValue(const QString &v) { if (v != m_value) { m_value = v; update(); } }
    void applyScale(qreal k);
    QSize sizeHint() const override;
    std::function<void(const QString &)> onChange;

signals:
    void changed(const QString &v);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    void pick(const QString &k);
    QString m_value = "medium";
    bool m_hover = false;
    qreal m_k = 1.0;
};

/* ------------------------------------------------------------------ 棋盘 */
class Board : public QWidget
{
    Q_OBJECT
public:
    explicit Board(Game *g, QWidget *parent = nullptr);
    void applyScale(qreal k);
    std::function<void()> onChanged;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    Game *m_g;
    qreal m_k = 1.0;
};

/* ------------------------------------------------------------------ 卡片弹窗 */
QString cardDialog(QWidget *parent, const QString &title, const QString &sub,
                   const QVector<QPair<QString, QString>> &stats,
                   const QString &primary, const QString &secondary);

/* 暂停界面：大号用时 + 圆形三角按钮。只有点三角才返回 true（=恢复计时） */
bool pauseDialog(QWidget *parent, const QString &time);

#endif // WIDGETS_H
