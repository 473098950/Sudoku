# 静态链接版：单文件 exe，不依赖任何 Qt / MinGW 运行时 DLL
# 用法：qmake sudoku_static.pro -spec win32-g++ "CONFIG+=release" && mingw32-make
QT += core gui widgets
CONFIG += c++17
CONFIG += static
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

RESOURCES += resources.qrc
RC_FILE = app.rc

# ---- 静态 Qt（本机自编：D:\Qt\static-build → D:\Qt\6.11.1\static-mingw）
#      换机器时用环境变量覆盖：SUDOKU_MINGW_BIN / SUDOKU_QT_STATIC
MINGW_BIN = $$(SUDOKU_MINGW_BIN)
isEmpty(MINGW_BIN): MINGW_BIN = D:/Qt/Tools/mingw1310_64/bin
QT_STATIC = $$(SUDOKU_QT_STATIC)
isEmpty(QT_STATIC): QT_STATIC = D:/Qt/6.11.1/static-mingw
QT_BIN    = $$QT_STATIC/bin
QT_LIB    = $$QT_STATIC/lib
QT_PLUGIN = $$QT_STATIC/plugins

QMAKE_CC     = $$MINGW_BIN/gcc.exe
QMAKE_CXX    = $$MINGW_BIN/g++.exe
QMAKE_LINK   = $$MINGW_BIN/g++.exe
QMAKE_LINK_C = $$MINGW_BIN/gcc.exe
QMAKE_RC     = $$MINGW_BIN/windres.exe
QMAKE_MOC    = $$QT_BIN/moc.exe
QMAKE_RCC    = $$QT_BIN/rcc.exe
QMAKE_UIC    = $$QT_BIN/uic.exe

# 把 Qt 的 lib / 插件目录喂给链接器
QMAKE_LIBDIR += $$QT_LIB
QMAKE_LIBDIR += $$QT_PLUGIN/platforms

# 静态链接 MinGW 运行时（libgcc / libstdc++ / winpthread 全部进 exe）
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

# 丢弃未引用的函数/数据段，显著减小静态体积
QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
QMAKE_CFLAGS   += -ffunction-sections -fdata-sections
QMAKE_LFLAGS   += -Wl,--gc-sections

# 平台插件：静态构建不加载 .dll 插件，必须显式链进来
QTPLUGIN.platforms = qwindows
QTPLUGIN += qwindows
