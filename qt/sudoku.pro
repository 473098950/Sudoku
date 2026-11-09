QT += core gui widgets
CONFIG += c++17
TARGET = Sudoku
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    widgets.cpp

HEADERS += \
    mainwindow.h \
    widgets.h \
    game.h \
    palette.h

RC_FILE = app.rc

# ---- 编译器路径：默认写死本机路径，避免 PATH 命中其它编译器（如 llvm-mingw 的伪 g++）
#      换机器时用环境变量覆盖：SUDOKU_MINGW_BIN / SUDOKU_QT_BIN
MINGW_BIN = $$(SUDOKU_MINGW_BIN)
isEmpty(MINGW_BIN): MINGW_BIN = D:/Qt/Tools/mingw1310_64/bin
QT_BIN = $$(SUDOKU_QT_BIN)
isEmpty(QT_BIN): QT_BIN = D:/Qt/6.11.1/mingw_64/bin
QMAKE_CC     = $$MINGW_BIN/gcc.exe
QMAKE_CXX    = $$MINGW_BIN/g++.exe
QMAKE_LINK   = $$MINGW_BIN/g++.exe
QMAKE_LINK_C = $$MINGW_BIN/gcc.exe
QMAKE_RC     = $$MINGW_BIN/windres.exe
QMAKE_MOC    = $$QT_BIN/moc.exe
QMAKE_RCC    = $$QT_BIN/rcc.exe
QMAKE_UIC    = $$QT_BIN/uic.exe
