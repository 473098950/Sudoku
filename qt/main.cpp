#include <QApplication>
#include <QTimer>
#include <QIcon>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QFile>

#include "mainwindow.h"
#include "widgets.h"

/* 说明：
   窗口默认是「无边框自绘」模式，这样才能缩到棋盘 80×80（整窗 86×104）。
   一旦切回带系统标题栏的模式，Windows 会强制客户区宽度 ≥120px，
   此时最小尺寸自动换成 120×145 —— 该判断在 Window::minScale() 里。
   WM_GETMINMAXINFO 的改写也在 Window::nativeEvent() 中按当前下限动态计算。 */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sudoku"));
    QApplication::setOrganizationName(QStringLiteral("Buddy"));
    app.setFont(uiFont(9.0));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));   // 内嵌资源，单文件可独立运行

    Window w;
    w.show();

    QString shotPath;
    qreal scale = 1.0;
    bool demo = false, notesDemo = false, scaleSet = false;

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--scale" && i + 1 < argc) {
            scale = QString::fromLocal8Bit(argv[++i]).toDouble();
            scaleSet = true;
        }
        else if (a == "--demo")
            demo = true;
        else if (a == "--demo-notes")
            notesDemo = true;
        else if (a == "--shot" && i + 1 < argc)
            shotPath = QString::fromLocal8Bit(argv[++i]);
    }

    if (demo) w.demoState();               // 注入中间态，仅用于自检截图
    if (notesDemo) w.demoNotes();           // 注入候选数笔记态，仅用于讲解截图
    if (scaleSet) w.setScale(scale);       // 显式指定了 --scale 就强制应用（含 1.0）

    if (qEnvironmentVariableIsSet("SUDOKU_DUMP")) {
        QFile f(QCoreApplication::applicationDirPath() + "/dump.txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(QString("window %1x%2\n").arg(w.width()).arg(w.height()).toUtf8());
            for (QWidget *c : w.findChildren<QWidget *>()) {
                const QRect g = c->geometry();
                f.write(QString("%1 [%2,%3 %4x%5]\n")
                            .arg(c->metaObject()->className())
                            .arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height()).toUtf8());
            }
        }
    }

    if (!shotPath.isEmpty()) {
        QTimer::singleShot(1200, [&w, shotPath] {
            w.grab().save(shotPath);
            QApplication::quit();
        });
    }
    return app.exec();
}
