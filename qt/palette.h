#ifndef PALETTE_H
#define PALETTE_H

#include <QColor>
#include <QList>
#include <QString>

// 配色令牌来自 Chrome 扩展 Sudoku 1.0.1 的 --brand-* / board 定义
struct Palette {
    QColor bg, surface, text, hint, accent, onAccent, entry, mistake, good;
    int area = 12, same = 26, selected = 40, conflict = 20;
};

struct ThemeDef {
    QString id;
    QString name;
    Palette dark;
    Palette light;
};

inline QColor mc(const char *s) { return QColor(QString::fromLatin1(s)); }

inline Palette makePal(const char *bg, const char *surface, const char *text, const char *hint,
                       const char *accent, const char *onAccent, const char *entry,
                       const char *mistake, const char *good,
                       int area, int same, int selected, int conflict)
{
    Palette p;
    p.bg = mc(bg);
    p.surface = mc(surface);
    p.text = mc(text);
    p.hint = mc(hint);
    p.accent = mc(accent);
    p.onAccent = mc(onAccent);
    p.entry = mc(entry);
    p.mistake = mc(mistake);
    p.good = mc(good);
    p.area = area;
    p.same = same;
    p.selected = selected;
    p.conflict = conflict;
    return p;
}

inline QList<ThemeDef> &themes()
{
    static QList<ThemeDef> list = {
        {"classic", "Classic",
         makePal("#161a29", "#21263a", "#eef0f8", "#8a90aa", "#5b67e8", "#ffffff",
                 "#a6aeff", "#ff6b72", "#2e9e5b", 12, 26, 40, 20),
         makePal("#ffffff", "#f1f2f7", "#181a2b", "#767b92", "#4a56d8", "#ffffff",
                 "#4a56d8", "#c42b2b", "#2e9e5b", 10, 22, 36, 20)},
        {"paper", "Paper",
         makePal("#23201b", "#2f2b25", "#efe8da", "#a1978a", "#86aedb", "#1b1914",
                 "#a9c8ec", "#ff8577", "#58c48f", 11, 24, 36, 26),
         makePal("#f8f3e8", "#ede6d6", "#2a2620", "#73695b", "#2f5d8a", "#ffffff",
                 "#2f5d8a", "#b0281f", "#1f7a52", 9, 22, 34, 28)},
        {"night", "Night",
         makePal("#13112a", "#1f1c3a", "#ece9ff", "#918bb8", "#8a78ff", "#110e24",
                 "#b4c4ff", "#ff7290", "#5fd1a7", 12, 28, 42, 28),
         makePal("#f4f1fb", "#e8e3f5", "#221d39", "#6b6489", "#6247d6", "#ffffff",
                 "#5238c4", "#b8233f", "#1f7f66", 9, 22, 34, 26)},
        {"garden", "Garden",
         makePal("#121b14", "#1c271e", "#e5efe2", "#8b9b87", "#5ab8c6", "#0c140e",
                 "#8fd3de", "#ff8070", "#d9b45f", 11, 26, 40, 30),
         makePal("#f4f6ee", "#e5ead9", "#1d2a1e", "#63715d", "#1f6873", "#ffffff",
                 "#1f6873", "#b3261e", "#7a5a12", 9, 20, 32, 32)},
        {"contrast", "Contrast",
         makePal("#000000", "#1a1a1a", "#ffffff", "#a6a6a6", "#ffd400", "#000000",
                 "#5cd2ff", "#ff5c7a", "#4fe0a0", 10, 22, 34, 34),
         makePal("#ffffff", "#ededed", "#000000", "#555555", "#0035c8", "#ffffff",
                 "#0035c8", "#a33a00", "#007a33", 10, 22, 36, 26)},
        {"arcade", "Arcade",
         makePal("#0e0b16", "#1b1628", "#f3edff", "#938aad", "#ff4fd8", "#16000f",
                 "#46eaff", "#ffb02e", "#5cff9d", 12, 26, 38, 38),
         makePal("#fbf7ff", "#efe7fb", "#1e1430", "#706287", "#7a3ff2", "#ffffff",
                 "#0a6fb0", "#b8213f", "#0f8f5e", 9, 22, 34, 26)},
    };
    return list;
}

// 当前生效的调色板
inline Palette &pal()
{
    static Palette p = themes().first().dark;
    return p;
}

inline QColor mixc(const QColor &a, const QColor &b, int pct)
{
    const double t = qBound(0, pct, 100) / 100.0;
    return QColor(qRound(a.red() * t + b.red() * (1 - t)),
                  qRound(a.green() * t + b.green() * (1 - t)),
                  qRound(a.blue() * t + b.blue() * (1 - t)));
}

inline QColor col(const QColor &c, double alpha = 1.0)
{
    QColor x = c;
    x.setAlphaF(alpha);
    return x;
}

inline QString hexa(const QColor &c, double a = 1.0)
{
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

#endif // PALETTE_H
