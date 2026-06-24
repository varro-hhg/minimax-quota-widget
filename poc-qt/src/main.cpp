// poc-qt/src/main.cpp
// MiniMax Coding Plan 配额桌面小部件 — Qt 5 入口

#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MiniMax Quota Widget");
    app.setOrganizationName("MiniMaxQuotaWidget");

    MainWindow w;
    w.show();
    return app.exec();
}
