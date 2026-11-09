#ifndef GAME_H
#define GAME_H

#include <QVector>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QSet>
#include <QString>
#include <functional>

static const int NCELLS = 81;

inline int maskAt(const QVector<int> &g, int i)
{
    const int r = i / 9, c = i % 9;
    const int br = r - r % 3, bc = c - c % 3;
    int m = 0;
    for (int k = 0; k < 9; ++k) {
        m |= 1 << g[r * 9 + k];
        m |= 1 << g[k * 9 + c];
    }
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            m |= 1 << g[(br + a) * 9 + bc + b];
    return m & ~1;
}

inline QVector<int> genFull()
{
    QVector<int> g(NCELLS, 0);
    std::function<bool(int)> go = [&](int i) -> bool {
        if (i == NCELLS) return true;
        const int used = maskAt(g, i);
        int ds[9];
        int n = 0;
        for (int d = 1; d <= 9; ++d)
            if (!(used & (1 << d))) ds[n++] = d;
        for (int k = n - 1; k > 0; --k) {
            const int j = QRandomGenerator::global()->bounded(k + 1);
            std::swap(ds[k], ds[j]);
        }
        for (int k = 0; k < n; ++k) {
            g[i] = ds[k];
            if (go(i + 1)) return true;
            g[i] = 0;
        }
        return false;
    };
    go(0);
    return g;
}

inline int countSolutions(QVector<int> &g, int limit)
{
    int best = -1, bestMask = 0, bestCnt = 10;
    for (int i = 0; i < NCELLS; ++i) {
        if (g[i]) continue;
        const int used = maskAt(g, i);
        int cnt = 0;
        for (int d = 1; d <= 9; ++d)
            if (!(used & (1 << d))) ++cnt;
        if (cnt == 0) return 0;
        if (cnt < bestCnt) {
            bestCnt = cnt;
            best = i;
            bestMask = used;
            if (cnt == 1) break;
        }
    }
    if (best < 0) return 1;
    int total = 0;
    for (int d = 1; d <= 9; ++d) {
        if (bestMask & (1 << d)) continue;
        g[best] = d;
        total += countSolutions(g, limit - total);
        g[best] = 0;
        if (total >= limit) return total;
    }
    return total;
}

struct Puzzle {
    QVector<int> puzzle;
    QVector<int> solution;
};

inline int givensFor(const QString &diff)
{
    if (diff == "easy") return 44;
    if (diff == "hard") return 29;
    if (diff == "expert") return 25;
    return 34;
}

inline Puzzle makePuzzle(const QString &diff)
{
    Puzzle out;
    out.solution = genFull();
    out.puzzle = out.solution;
    const int target = NCELLS - givensFor(diff);

    QVector<int> order(NCELLS);
    for (int i = 0; i < NCELLS; ++i) order[i] = i;
    for (int k = NCELLS - 1; k > 0; --k) {
        const int j = QRandomGenerator::global()->bounded(k + 1);
        std::swap(order[k], order[j]);
    }

    QElapsedTimer t;
    t.start();
    int removed = 0;
    for (int idx : order) {
        if (removed >= target || t.elapsed() > 1600) break;
        const int bak = out.puzzle[idx];
        if (!bak) continue;
        out.puzzle[idx] = 0;
        QVector<int> probe = out.puzzle;
        if (countSolutions(probe, 2) != 1) out.puzzle[idx] = bak;
        else ++removed;
    }
    return out;
}

inline QVector<int> peersOf(int i)
{
    const int r = i / 9, c = i % 9;
    const int br = r - r % 3, bc = c - c % 3;
    QVector<int> out;
    out.reserve(21);
    for (int k = 0; k < 9; ++k) {
        out << r * 9 + k << k * 9 + c;
    }
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            out << (br + a) * 9 + bc + b;
    return out;
}

struct Snapshot {
    QVector<int> values, notes, hinted;
    int mistakes = 0;
};

class Game
{
public:
    QVector<int> puzzle, solution, values, notes, hinted;
    int sel = -1;
    bool notesMode = false;
    int mistakes = 0, seconds = 0, hints = 3;
    QString diff = "medium";
    bool done = false, running = true;
    // 暂停：为 true 时计时器不走，且不接受落子输入（点计时器胶囊 / 离开窗口时置位）
    bool paused = false;
    QVector<Snapshot> history;

    Game() { reset(); }

    void reset()
    {
        puzzle = QVector<int>(NCELLS, 0);
        solution = QVector<int>(NCELLS, 0);
        values = QVector<int>(NCELLS, 0);
        notes = QVector<int>(NCELLS, 0);
        hinted = QVector<int>(NCELLS, 0);
    }

    void newGame(const QString &d)
    {
        Puzzle p = makePuzzle(d);
        puzzle = p.puzzle;
        solution = p.solution;
        values = puzzle;
        notes = QVector<int>(NCELLS, 0);
        hinted = QVector<int>(NCELLS, 0);
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

    void snap()
    {
        history.append({values, notes, hinted, mistakes});
        if (history.size() > 300) history.removeFirst();
    }

    bool undo()
    {
        if (history.isEmpty()) return false;
        const Snapshot s = history.takeLast();
        values = s.values;
        notes = s.notes;
        hinted = s.hinted;
        mistakes = s.mistakes;
        return true;
    }

    void place(int d)
    {
        const int i = sel;
        if (i < 0 || done || puzzle[i]) return;
        if (notesMode) {
            if (values[i]) return;
            snap();
            notes[i] ^= (1 << (d - 1));
            return;
        }
        if (values[i] == d) {
            snap();
            values[i] = 0;
            hinted[i] = 0;
            return;
        }
        snap();
        values[i] = d;
        notes[i] = 0;
        if (d == solution[i]) {
            hinted[i] = 0;
            const int keep = ~(1 << (d - 1));
            for (int p : peersOf(i)) notes[p] &= keep;
        } else {
            ++mistakes;
        }
    }

    void erase()
    {
        const int i = sel;
        if (i < 0 || done || puzzle[i]) return;
        if (!values[i] && !notes[i]) return;
        snap();
        values[i] = 0;
        notes[i] = 0;
        hinted[i] = 0;
    }

    bool hint()
    {
        if (done || hints <= 0) return false;
        int i = sel;
        if (i < 0 || puzzle[i] || values[i] == solution[i]) {
            QVector<int> empty;
            for (int k = 0; k < NCELLS; ++k)
                if (!puzzle[k] && values[k] != solution[k]) empty << k;
            if (empty.isEmpty()) return false;
            i = empty[QRandomGenerator::global()->bounded(empty.size())];
            sel = i;
        }
        snap();
        --hints;
        values[i] = solution[i];
        hinted[i] = 1;
        notes[i] = 0;
        const int keep = ~(1 << (solution[i] - 1));
        for (int p : peersOf(i)) notes[p] &= keep;
        return true;
    }

    bool solved() const
    {
        for (int i = 0; i < NCELLS; ++i)
            if (values[i] != solution[i]) return false;
        return true;
    }

    QSet<int> conflicts() const
    {
        QSet<int> bad;
        for (int i = 0; i < NCELLS; ++i) {
            const int v = values[i];
            if (!v) continue;
            const int r = i / 9, c = i % 9;
            for (int k = 0; k < 9; ++k) {
                const int a = r * 9 + k, b = k * 9 + c;
                if (a != i && values[a] == v) { bad.insert(i); bad.insert(a); }
                if (b != i && values[b] == v) { bad.insert(i); bad.insert(b); }
            }
            const int br = r - r % 3, bc = c - c % 3;
            for (int x = 0; x < 3; ++x)
                for (int y = 0; y < 3; ++y) {
                    const int p = (br + x) * 9 + bc + y;
                    if (p != i && values[p] == v) { bad.insert(i); bad.insert(p); }
                }
        }
        return bad;
    }

    // 每个数字还剩几个没放
    QVector<int> remaining() const
    {
        QVector<int> used(10, 0);
        for (int v : values)
            if (v) ++used[v];
        QVector<int> left(10, 0);
        for (int d = 1; d <= 9; ++d) left[d] = 9 - used[d];
        return left;
    }
};

#endif // GAME_H
