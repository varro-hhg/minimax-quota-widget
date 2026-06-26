QT       += core gui widgets network
CONFIG   += c++17
CONFIG   -= app_bundle

TARGET   = MiniMaxQuotaWidget
TEMPLATE = app

# Windows 子系统：debug 模式用 console 查看日志，release 用 windows
debug:win32: CONFIG += console
release:win32: CONFIG += windows

DEFINES += UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN \
           QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/QuotaConfig.cpp \
    src/MiniMaxClient.cpp \
    src/VolcengineClient.cpp \
    src/QuotaView.cpp \
    src/ProgressBar.cpp \
    src/TrayIcon.cpp

HEADERS += \
    src/MainWindow.h \
    src/QuotaConfig.h \
    src/MiniMaxClient.h \
    src/VolcengineClient.h \
    src/QuotaView.h \
    src/ProgressBar.h \
    src/TrayIcon.h

# 输出到 build/bin
DESTDIR     = build/bin
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui

# Windows：使用 user32 等系统库
win32: LIBS += -luser32 -lgdi32 -ldwmapi
RC_ICONS = src/assets/icon.ico
