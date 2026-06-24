QT       += core gui widgets network
CONFIG   += c++17
CONFIG   -= app_bundle

TARGET   = MiniMaxQuotaWidget
TEMPLATE = app

# Windows 子系统：不弹控制台窗口
win32: CONFIG += windows

DEFINES += UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN \
           QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp

HEADERS += \
    src/MainWindow.h

# 输出到 build/bin
DESTDIR     = build/bin
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

# Windows：使用 user32 等系统库
win32: LIBS += -luser32 -lgdi32 -ldwmapi
