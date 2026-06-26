// poc-qt/src/TrayIcon.h
// 系统托盘：菜单 + 激活信号（复用 D:\projects\calendar 模式）

#pragma once

#include <QObject>

class QSystemTrayIcon;
class QMenu;
class QAction;

class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    void setVisible(bool v);
    void setPinned(bool pinned);
    void refreshMenu();

signals:
    void showRequested();
    void refreshRequested();
    void togglePinRequested();
    void openConfigRequested();
    void quitRequested();

private slots:
    void onActivated(int reason);

private:
    QSystemTrayIcon* icon_ = nullptr;
    QMenu*           menu_ = nullptr;
    QAction*         pinAction_   = nullptr;
    bool             pinned_      = true;
    bool             visible_     = false;
};
