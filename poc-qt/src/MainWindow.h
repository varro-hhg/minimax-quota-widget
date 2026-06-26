// poc-qt/src/MainWindow.h
// 主窗口：装载 QuotaView / MiniMaxClient / VolcengineClient / TrayIcon
// 负责：托盘信号、刷新调度、closeEvent（隐藏到托盘）、置顶切换

#pragma once

#include <QPoint>
#include <QWidget>

#include "MiniMaxClient.h"   // brings MiniMaxModel + client
#include "VolcengineClient.h" // brings VolcUsage + client

class QTimer;
class QVBoxLayout;
class QuotaConfig;
class QuotaView;
class TrayIcon;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void refreshQuota();
    void onMiniMaxReady(QList<MiniMaxModel> models);
    void onMiniMaxError(QString msg);
    void onVolcReady(VolcUsage usage);
    void onVolcError(QString msg);
    void onCountdownTick();

    // tray signals
    void onTrayShow();
    void onTrayRefresh();
    void onTrayTogglePin();
    void onTrayOpenConfig();
    void onTrayQuit();

private:
    void scheduleNextRefresh(qint64 generalResetMs);
    void togglePin();

    // UI
    QuotaView*    view_ = nullptr;

    // Logic
    QuotaConfig*      config_      = nullptr;
    MiniMaxClient*    minimax_     = nullptr;
    VolcengineClient* volc_        = nullptr;
    TrayIcon*         tray_        = nullptr;

    // Timers
    QTimer* refreshTimer_   = nullptr;  // 5min / 5s adaptive
    QTimer* countdownTimer_ = nullptr;  // 30s tick

    // State
    bool isQuitting_      = false;
    bool isPinned_        = true;
    QPoint dragStartPos_;
    bool  dragging_       = false;
};
