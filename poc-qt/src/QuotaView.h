// poc-qt/src/QuotaView.h
// 配额数据展示视图：Volcengine / MiniMax 5h / Weekly 三个 section + 状态栏

#pragma once

#include "MiniMaxClient.h"
#include "VolcengineClient.h"

#include <QHash>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class ProgressBar;
class QVBoxLayout;

class QuotaView : public QWidget
{
    Q_OBJECT

public:
    explicit QuotaView(QWidget* parent = nullptr);

    void setConfigPath(const QString& path, bool created);
    void setMiniMaxData(QList<MiniMaxModel> models);
    void setVolcData(VolcUsage usage);
    void setStatus(const QString& message, bool online, bool idle = false);
    void setError(const QString& message, bool isNoConfig);
    void setPinned(bool pinned);

    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

    // 30s tick — 减少 intervalResetsInMs
    void tickCountdown();

    static QString formatDuration(qint64 ms);
    static QString formatBoost(int permille);
    static QString formatDateShort(qint64 epochMs);

signals:
    void closeRequested();
    void pinToggled();
    void openConfigRequested();

private:
    void setupUi();
    void renderMiniMaxSection();

    // Config overlay
    QWidget* noConfigOverlay_   = nullptr;

    // Volc
    QWidget*  volcSection_      = nullptr;
    QLabel*   volcStatus_       = nullptr;
    QVBoxLayout* volcList_      = nullptr;

    // MiniMax
    QLabel*   intervalReset_    = nullptr;
    QLabel*   weeklyRange_      = nullptr;
    QVBoxLayout* intervalList_  = nullptr;
    QVBoxLayout* weeklyList_    = nullptr;

    // Footer
    QLabel*   statusDot_        = nullptr;
    QLabel*   statusText_       = nullptr;

    // Title bar buttons
    QPushButton* pinBtn_        = nullptr;
    QPushButton* closeBtn_      = nullptr;
    bool         isPinned_      = true;

    // Data cache
    QList<MiniMaxModel> models_;
    qint64   generalIntervalMs_ = 0;   // 用于 30s 倒计时
    qint64   generalWeeklyStart_ = 0;
    qint64   generalWeeklyEnd_   = 0;

    QString  configPath_;
    bool     configCreated_     = false;

    // For tick: every MiniMax model row by name, to update boost/reset labels
    QHash<QString, ProgressBar*> intervalBars_;
    QHash<QString, ProgressBar*> weeklyBars_;
};
