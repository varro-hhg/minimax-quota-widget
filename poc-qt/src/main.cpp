// poc-qt/src/main.cpp
// MiniMax Coding Plan 配额桌面小部件 — Qt 5 入口

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QFile f(QDir::homePath() + "/.minimax-quota-widget/qt_debug.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "=== main() START ===\n";
        f.close();
    }

    QApplication app(argc, argv);
    app.setApplicationName("MiniMax Quota Widget");
    app.setOrganizationName("MiniMaxQuotaWidget");
    app.setQuitOnLastWindowClosed(false);  // 隐藏窗口后保持托盘存活

    MainWindow w;
    w.show();
    return app.exec();
}
