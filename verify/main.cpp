/* 独立验证工具：批量生成题目并统计唯一解 / 给数分布 / 耗时。
   include 的正是主程序用的 ../qt/game.h，验证的是同一份生成器。
   输出 puzzles.txt：<81 puzzl> <81 solution> <diff> <解数>
*/
#include <QtCore>
#include <cstdio>
#include "../qt/game.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    /* 模式二：--probe <file>
       文件每行 81 个字符（0=空），逐行输出该题的解数（上限 3）。
       用于和 Python 实现交叉验证「多解题」的判定是否一致。 */
    if (argc > 2 && QString::fromLatin1(argv[1]) == "--probe") {
        QFile in(QString::fromLocal8Bit(argv[2]));
        if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) return 1;
        while (!in.atEnd()) {
            const QByteArray ln = in.readLine().trimmed();
            if (ln.size() < 81) continue;
            QVector<int> g(NCELLS, 0);
            for (int i = 0; i < NCELLS; ++i) g[i] = ln.at(i) - '0';
            QVector<int> probe = g;
            printf("%d\n", countSolutions(probe, 3));
        }
        return 0;
    }

    const int N = argc > 1 ? QString::fromLatin1(argv[1]).toInt() : 40;

    const QVector<QString> diffs = {"easy", "medium", "hard", "expert"};
    FILE *f = fopen("puzzles.txt", "w");
    if (!f) return 1;

    for (const QString &d : diffs) {
        int bad = 0, minG = 99, maxG = 0, sumG = 0;
        qint64 ms = 0, maxMs = 0, hitTimeout = 0;
        for (int i = 0; i < N; ++i) {
            QElapsedTimer t; t.start();
            const Puzzle p = makePuzzle(d);
            const qint64 el = t.elapsed();
            ms += el;
            maxMs = qMax(maxMs, el);
            if (el >= 1600) ++hitTimeout;

            QVector<int> probe = p.puzzle;
            const int n = countSolutions(probe, 2);   // 生成器自身的判定
            if (n != 1) ++bad;

            int g = 0, mismatch = 0;
            for (int k = 0; k < NCELLS; ++k) {
                if (p.puzzle[k]) {
                    ++g;
                    if (p.puzzle[k] != p.solution[k]) ++mismatch;
                }
            }
            if (mismatch) ++bad;
            minG = qMin(minG, g);
            maxG = qMax(maxG, g);
            sumG += g;

            QByteArray line;
            for (int k = 0; k < NCELLS; ++k) line += char('0' + p.puzzle[k]);
            line += ' ';
            for (int k = 0; k < NCELLS; ++k) line += char('0' + p.solution[k]);
            line += ' ';
            line += d.toLatin1();
            line += ' ';
            line += QByteArray::number(n);
            fprintf(f, "%s\n", line.constData());
        }
        fflush(f);
        printf("%-7s n=%-3d bad=%-2d givens=%d..%d(avg %.1f) time avg=%lldms max=%lldms timeout=%lld\n",
               d.toLatin1().constData(), N, bad, minG, maxG, double(sumG) / N,
               ms / N, maxMs, hitTimeout);
    }
    fclose(f);
    return 0;
}
