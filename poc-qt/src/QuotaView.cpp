// poc-qt/src/QuotaView.cpp

#include "QuotaView.h"
#include "ProgressBar.h"
#include "QuotaConfig.h"

#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
namespace {
constexpr int kCornerRadius = 18;
}

QuotaView::QuotaView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setupUi();
}

void QuotaView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(8);

    // ── Titlebar (drag region) ──
    auto* titleBar = new QHBoxLayout;
    auto* title = new QLabel(QString::fromUtf8("CODING PLAN 配额"), this);
    title->setStyleSheet(
        "color: rgba(255,255,255,0.75);"
        "font-size: 14px; font-weight: 700;");
    titleBar->addWidget(title);
    titleBar->addStretch();

    // ── Pin button ──
    pinBtn_ = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x8c"), this);
    pinBtn_->setFixedSize(28, 28);
    pinBtn_->setCursor(Qt::PointingHandCursor);
    pinBtn_->setStyleSheet(
        "QPushButton { background: rgba(99,102,241,0.4); border: none; border-radius: 14px; "
        "font-size: 14px; }"
        "QPushButton:hover { background: rgba(99,102,241,0.6); }");
    pinBtn_->setToolTip(QString::fromUtf8("取消置顶"));
    titleBar->addWidget(pinBtn_);
    titleBar->addSpacing(4);

    // ── Close button ──
    closeBtn_ = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"), this);
    closeBtn_->setFixedSize(28, 28);
    closeBtn_->setCursor(Qt::PointingHandCursor);
    closeBtn_->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.1); border: none; border-radius: 14px; "
        "color: rgba(255,255,255,0.7); font-size: 13px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(239,68,68,0.8); color: white; }");
    closeBtn_->setToolTip(QString::fromUtf8("关闭到托盘"));
    titleBar->addWidget(closeBtn_);

    root->addLayout(titleBar);

    connect(pinBtn_, &QPushButton::clicked, this, &QuotaView::pinToggled);
    connect(closeBtn_, &QPushButton::clicked, this, &QuotaView::closeRequested);

    // ── Volcengine section ──
    volcSection_ = new QWidget(this);
    auto* volcLay = new QVBoxLayout(volcSection_);
    volcLay->setContentsMargins(0, 0, 0, 0);
    volcLay->setSpacing(4);

    auto* volcHeader = new QHBoxLayout;
    auto* volcLabel  = new QLabel(QString::fromUtf8("🚀 火山方舟 Coding Plan"), volcSection_);
    volcLabel->setStyleSheet("color: rgba(255,255,255,0.55); font-size: 13px;");
    volcStatus_ = new QLabel(QString::fromUtf8("—"), volcSection_);
    volcStatus_->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 12px;");
    volcHeader->addWidget(volcLabel);
    volcHeader->addStretch();
    volcHeader->addWidget(volcStatus_);
    volcLay->addLayout(volcHeader);

    auto* volcContainer = new QWidget(volcSection_);
    volcContainer->setStyleSheet(
        "background: rgba(255,255,255,0.04);"
        "border: 1px solid rgba(255,255,255,0.07);"
        "border-radius: 10px;");
    auto* volcContainerLay = new QVBoxLayout(volcContainer);
    volcContainerLay->setContentsMargins(0, 0, 0, 0);
    volcContainerLay->setSpacing(0);
    volcList_ = volcContainerLay;
    volcLay->addWidget(volcContainer);
    root->addWidget(volcSection_);

    // ── MiniMax group label ──
    auto* mmGroup = new QLabel(QString::fromUtf8("🤖 MiniMax Coding Plan"), this);
    mmGroup->setStyleSheet(
        "color: rgba(255,255,255,0.45); font-size: 13px;"
        "padding: 8px 0 4px; border-top: 1px solid rgba(255,255,255,0.08);");
    root->addWidget(mmGroup);

    // ── 5h interval ──
    auto* intervalHeader = new QHBoxLayout;
    auto* intLabel = new QLabel(QString::fromUtf8("⏰ 5h 窗口"), this);
    intLabel->setStyleSheet("color: rgba(255,255,255,0.55); font-size: 13px;");
    intervalReset_ = new QLabel(QString::fromUtf8("—"), this);
    intervalReset_->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 12px;");
    intervalHeader->addWidget(intLabel);
    intervalHeader->addStretch();
    intervalHeader->addWidget(intervalReset_);
    root->addLayout(intervalHeader);

    auto* intContainer = new QWidget(this);
    intContainer->setStyleSheet(
        "background: rgba(255,255,255,0.04);"
        "border: 1px solid rgba(255,255,255,0.07);"
        "border-radius: 10px;");
    auto* intLay = new QVBoxLayout(intContainer);
    intLay->setContentsMargins(0, 0, 0, 0);
    intLay->setSpacing(0);
    intervalList_ = intLay;
    root->addWidget(intContainer);

    // ── Weekly ──
    auto* weeklyHeader = new QHBoxLayout;
    auto* wkLabel = new QLabel(QString::fromUtf8("📅 本周"), this);
    wkLabel->setStyleSheet("color: rgba(255,255,255,0.55); font-size: 13px;");
    weeklyRange_ = new QLabel(QString::fromUtf8("—"), this);
    weeklyRange_->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 12px;");
    weeklyHeader->addWidget(wkLabel);
    weeklyHeader->addStretch();
    weeklyHeader->addWidget(weeklyRange_);
    root->addLayout(weeklyHeader);

    auto* wkContainer = new QWidget(this);
    wkContainer->setStyleSheet(
        "background: rgba(255,255,255,0.04);"
        "border: 1px solid rgba(255,255,255,0.07);"
        "border-radius: 10px;");
    auto* wkLay = new QVBoxLayout(wkContainer);
    wkLay->setContentsMargins(0, 0, 0, 0);
    wkLay->setSpacing(0);
    weeklyList_ = wkLay;
    root->addWidget(wkContainer);

    root->addStretch();

    // ── Footer ──
    auto* footer = new QHBoxLayout;
    statusDot_ = new QLabel(this);
    statusDot_->setFixedSize(8, 8);
    statusDot_->setStyleSheet(
        "background: rgba(255,255,255,0.25);"
        "border-radius: 4px;");
    statusText_ = new QLabel(QString::fromUtf8("等待数据…"), this);
    statusText_->setStyleSheet("color: rgba(255,255,255,0.4); font-size: 12px;");
    auto* autoLabel = new QLabel(QString::fromUtf8("自动 5m"), this);
    autoLabel->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 12px; margin-left: auto;");
    footer->addWidget(statusDot_);
    footer->addWidget(statusText_);
    footer->addWidget(autoLabel);
    root->addLayout(footer);

    // ── No-config overlay ──
    noConfigOverlay_ = new QWidget(this);
    noConfigOverlay_->setStyleSheet("background: rgba(18,18,28,0.92);");
    noConfigOverlay_->hide();
    noConfigOverlay_->setAttribute(Qt::WA_TranslucentBackground);

    auto* ovLay = new QVBoxLayout(noConfigOverlay_);
    ovLay->setContentsMargins(32, 28, 32, 28);
    ovLay->setSpacing(10);
    ovLay->setAlignment(Qt::AlignCenter);

    auto* icon = new QLabel(QString::fromUtf8("🔑"), noConfigOverlay_);
    icon->setStyleSheet("font-size: 36px;");
    icon->setAlignment(Qt::AlignCenter);
    ovLay->addWidget(icon);

    auto* title2 = new QLabel(QString::fromUtf8("请配置 Coding Plan"), noConfigOverlay_);
    title2->setStyleSheet("color: rgba(255,255,255,0.9); font-size: 16px; font-weight: 600;");
    title2->setAlignment(Qt::AlignCenter);
    ovLay->addWidget(title2);

    auto* body = new QLabel(
        QString::fromUtf8("已自动创建示例配置文件，请填入凭证后刷新。"),
        noConfigOverlay_);
    body->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 13px;");
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    ovLay->addWidget(body);

    auto* code = new QLabel(configPath_, noConfigOverlay_);
    code->setStyleSheet(
        "color: rgba(200,200,255,0.8); font-size: 12px;"
        "background: rgba(255,255,255,0.08); padding: 4px 8px;"
        "border-radius: 4px; font-family: 'Cascadia Code','Consolas',monospace;");
    code->setAlignment(Qt::AlignCenter);
    code->setWordWrap(true);
    ovLay->addWidget(code);

    auto* actions = new QHBoxLayout;
    actions->setAlignment(Qt::AlignCenter);
    auto* btn1 = new QPushButton(QString::fromUtf8("📂 打开配置文件"), noConfigOverlay_);
    auto* btn2 = new QPushButton(QString::fromUtf8("📁 打开文件夹"), noConfigOverlay_);
    QString btnStyle =
        "QPushButton{ padding: 6px 14px; border: 1px solid rgba(255,255,255,0.15);"
        "border-radius: 8px; background: rgba(255,255,255,0.08);"
        "color: rgba(255,255,255,0.85); font-size: 13px; }"
        "QPushButton:hover{ background: rgba(255,255,255,0.16); }";
    btn1->setStyleSheet(btnStyle);
    btn2->setStyleSheet(btnStyle);
    actions->addWidget(btn1);
    actions->addWidget(btn2);
    ovLay->addLayout(actions);

    connect(btn1, &QPushButton::clicked, this, [this]() {
        if (configPath_.isEmpty()) return;
        QFileInfo fi(configPath_);
        if (fi.exists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(configPath_));
        else
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
    });
    connect(btn2, &QPushButton::clicked, this, [this]() {
        QFileInfo fi(configPath_);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
    });
}

void QuotaView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (noConfigOverlay_) noConfigOverlay_->setGeometry(rect());
}

void QuotaView::paintEvent(QPaintEvent* /*event*/)
{
    // No-op: title bar is parent-level, but we still need translucency
}

void QuotaView::setConfigPath(const QString& path, bool created)
{
    configPath_   = path;
    configCreated_ = created;
    if (noConfigOverlay_) {
        auto* code = noConfigOverlay_->findChild<QLabel*>();
        if (code) code->setText(path);
    }
}

void QuotaView::setMiniMaxData(QList<MiniMaxModel> models)
{
    models_ = models;

    // 清空旧 rows
    QLayoutItem* it;
    while ((it = intervalList_->takeAt(0)) != nullptr) {
        delete it->widget();
        delete it;
    }
    while ((it = weeklyList_->takeAt(0)) != nullptr) {
        delete it->widget();
        delete it;
    }
    intervalBars_.clear();
    weeklyBars_.clear();

    // 固定顺序: general, video, speech, image
    const QStringList order = {"general", "video", "speech", "image"};
    QList<MiniMaxModel> sorted = models;
    std::sort(sorted.begin(), sorted.end(), [&](const MiniMaxModel& a, const MiniMaxModel& b) {
        int ia = order.indexOf(a.modelName); if (ia < 0) ia = 99;
        int ib = order.indexOf(b.modelName); if (ib < 0) ib = 99;
        return ia < ib;
    });

    generalIntervalMs_   = 0;
    generalWeeklyStart_  = 0;
    generalWeeklyEnd_    = 0;

    for (const MiniMaxModel& m : sorted) {
        // 5h interval row
        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(8, 6, 8, 6);
        rowLay->setSpacing(8);

        // Color dot
        auto* dot = new QLabel;
        dot->setFixedSize(8, 8);
        QString c = QuotaConfig::colorFor(m.intervalRemainPercent);
        QString dotColor =
            (c == "red")   ? "#f87171" :
            (c == "yellow")? "#facc15" : "#4ade80";
        dot->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(dotColor));
        rowLay->addWidget(dot);

        // Model name
        auto* name = new QLabel(m.modelName);
        name->setStyleSheet("color: rgba(255,255,255,0.88); font-size: 14px;");
        name->setFixedWidth(58);
        rowLay->addWidget(name);

        // Percent
        auto* pct = new QLabel(QString::number(m.intervalRemainPercent) + "%");
        pct->setStyleSheet("color: rgba(255,255,255,0.92); font-size: 14px; font-weight: 600;");
        pct->setFixedWidth(34);
        pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLay->addWidget(pct);

        // Bar
        auto* bar = new ProgressBar;
        bar->setPercent(m.intervalRemainPercent);
        bar->setBoostLabel(QuotaView::formatBoost(m.intervalBoostPermille));
        QString resetLabel = (m.modelName == sorted.first().modelName)
            ? formatDuration(m.intervalResetsInMs) : QString();
        bar->setResetLabel(resetLabel);
        rowLay->addWidget(bar, 1);

        // 容器圆角底
        auto* container = new QWidget;
        auto* contLay = new QVBoxLayout(container);
        contLay->setContentsMargins(0, 0, 0, 0);
        contLay->setSpacing(0);
        contLay->addWidget(row);
        // 底部边框
        container->setStyleSheet("border-bottom: 1px solid rgba(255,255,255,0.04);");
        intervalList_->addWidget(container);
        intervalBars_[m.modelName] = bar;

        if (m.modelName == "general") {
            generalIntervalMs_ = m.intervalResetsInMs;
            generalWeeklyStart_ = m.weeklyStart;
            generalWeeklyEnd_   = m.weeklyEnd;
        }

        // Weekly row
        auto* wrow = new QWidget;
        auto* wrowLay = new QHBoxLayout(wrow);
        wrowLay->setContentsMargins(8, 4, 8, 4);
        wrowLay->setSpacing(8);

        auto* wdot = new QLabel;
        wdot->setFixedSize(8, 8);
        QString wc = QuotaConfig::colorFor(m.weeklyRemainPercent);
        QString wdotColor =
            (wc == "red")   ? "#f87171" :
            (wc == "yellow")? "#facc15" : "#4ade80";
        wdot->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(wdotColor));
        wrowLay->addWidget(wdot);

        auto* wname = new QLabel(m.modelName);
        wname->setStyleSheet("color: rgba(255,255,255,0.88); font-size: 14px;");
        wname->setFixedWidth(58);
        wrowLay->addWidget(wname);

        auto* wpct = new QLabel(QString::number(m.weeklyRemainPercent) + "%");
        wpct->setStyleSheet("color: rgba(255,255,255,0.92); font-size: 14px; font-weight: 600;");
        wpct->setFixedWidth(34);
        wpct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        wrowLay->addWidget(wpct);

        auto* wbar = new ProgressBar;
        wbar->setPercent(m.weeklyRemainPercent);
        wbar->setBoostLabel(QuotaView::formatBoost(m.weeklyBoostPermille));
        wrowLay->addWidget(wbar, 1);

        auto* wcontainer = new QWidget;
        auto* wcontLay = new QVBoxLayout(wcontainer);
        wcontLay->setContentsMargins(0, 0, 0, 0);
        wcontLay->setSpacing(0);
        wcontLay->addWidget(wrow);
        wcontainer->setStyleSheet("border-bottom: 1px solid rgba(255,255,255,0.04);");
        weeklyList_->addWidget(wcontainer);
        weeklyBars_[m.modelName] = wbar;
    }

    // 去掉最后一个容器的底边
    int n = intervalList_->count();
    if (n > 0) {
        QLayoutItem* last = intervalList_->itemAt(n - 1);
        if (last && last->widget()) last->widget()->setStyleSheet("");
    }
    n = weeklyList_->count();
    if (n > 0) {
        QLayoutItem* last = weeklyList_->itemAt(n - 1);
        if (last && last->widget()) last->widget()->setStyleSheet("");
    }

    // 更新 meta
    if (generalIntervalMs_ > 0) {
        intervalReset_->setText(QString::fromUtf8("重置于 ") + formatDuration(generalIntervalMs_));
    } else {
        intervalReset_->setText("—");
    }
    if (generalWeeklyStart_ > 0 && generalWeeklyEnd_ > 0) {
        weeklyRange_->setText(QString::fromUtf8("%1 → %2")
            .arg(formatDateShort(generalWeeklyStart_))
            .arg(formatDateShort(generalWeeklyEnd_)));
    } else {
        weeklyRange_->setText("—");
    }
}

void QuotaView::setVolcData(VolcUsage usage)
{
    // 清空旧 rows
    QLayoutItem* it;
    while ((it = volcList_->takeAt(0)) != nullptr) {
        delete it->widget();
        delete it;
    }

    volcStatus_->setText(usage.status == QString::fromUtf8("Running")
        ? QString::fromUtf8("运行中") : usage.status);

    for (const VolcQuota& q : usage.quotas) {
        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(8, 6, 8, 6);
        rowLay->setSpacing(8);

        auto* dot = new QLabel;
        dot->setFixedSize(8, 8);
        int pct = static_cast<int>(std::round(q.remainPercent));
        QString c = QuotaConfig::colorFor(pct);
        QString dotColor =
            (c == "red")   ? "#f87171" :
            (c == "yellow")? "#facc15" : "#4ade80";
        dot->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(dotColor));
        rowLay->addWidget(dot);

        auto* name = new QLabel(q.label);
        name->setStyleSheet("color: rgba(255,255,255,0.88); font-size: 13px;");
        name->setFixedWidth(42);
        rowLay->addWidget(name);

        auto* pctLbl = new QLabel(QString::number(pct) + "%");
        pctLbl->setStyleSheet("color: rgba(255,255,255,0.92); font-size: 14px; font-weight: 600;");
        pctLbl->setFixedWidth(34);
        pctLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLay->addWidget(pctLbl);

        auto* bar = new ProgressBar;
        bar->setPercent(pct);
        if (q.resetsInMs > 0) bar->setResetLabel(formatDuration(q.resetsInMs));
        rowLay->addWidget(bar, 1);

        auto* container = new QWidget;
        auto* contLay = new QVBoxLayout(container);
        contLay->setContentsMargins(0, 0, 0, 0);
        contLay->setSpacing(0);
        contLay->addWidget(row);
        container->setStyleSheet("border-bottom: 1px solid rgba(255,255,255,0.04);");
        volcList_->addWidget(container);
    }

    int n = volcList_->count();
    if (n > 0) {
        QLayoutItem* last = volcList_->itemAt(n - 1);
        if (last && last->widget()) last->widget()->setStyleSheet("");
    }
}

void QuotaView::setStatus(const QString& message, bool online, bool idle)
{
    if (statusText_) statusText_->setText(message);
    QString c;
    if (idle)         c = "rgba(255,255,255,0.25)";
    else if (online)  c = "#4ade80";
    else              c = "#f87171";
    if (statusDot_)
        statusDot_->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(c));
}

void QuotaView::setError(const QString& message, bool isNoConfig)
{
    setStatus(message, false);
    if (isNoConfig && noConfigOverlay_) {
        noConfigOverlay_->show();
        noConfigOverlay_->raise();
    } else if (noConfigOverlay_) {
        noConfigOverlay_->hide();
    }
}

void QuotaView::tickCountdown()
{
    if (generalIntervalMs_ > 0) {
        generalIntervalMs_ -= 30 * 1000;
        if (generalIntervalMs_ < 0) generalIntervalMs_ = 0;
        if (intervalReset_)
            intervalReset_->setText(QString::fromUtf8("重置于 ") + formatDuration(generalIntervalMs_));

        // 更新第一行 interval bar 的 reset label
        if (intervalBars_.contains("general")) {
            qDebug() << "[view] updating general bar reset label";
            ProgressBar* bar = intervalBars_["general"];
            if (bar) bar->setResetLabel(formatDuration(generalIntervalMs_));
        } else {
            qDebug() << "[view] general bar NOT found in intervalBars_";
        }
    }
}

// ── Static helpers ──

QString QuotaView::formatDuration(qint64 ms)
{
    if (ms <= 0) return QString::fromUtf8("—");
    qint64 d = ms / 86'400'000LL;
    qint64 h = (ms % 86'400'000LL) / 3'600'000LL;
    qint64 m = (ms % 3'600'000LL) / 60'000LL;
    if (d > 0) return QString("%1d %2h").arg(d).arg(h);
    if (h > 0) return QString("%1h %2m").arg(h).arg(m);
    return QString("%1m").arg(m);
}

QString QuotaView::formatBoost(int permille)
{
    if (permille <= 1000) return QString();
    int pct = (permille - 1000 + 5) / 10;  // round
    return QString("+%1%").arg(pct);
}

QString QuotaView::formatDateShort(qint64 epochMs)
{
    if (epochMs <= 0) return QString::fromUtf8("—");
    QDateTime d = QDateTime::fromMSecsSinceEpoch(epochMs);
    return QString("%1-%2")
        .arg(d.date().month(), 2, 10, QChar('0'))
        .arg(d.date().day(),   2, 10, QChar('0'));
}

void QuotaView::setPinned(bool pinned)
{
    isPinned_ = pinned;
    if (pinBtn_) {
        pinBtn_->setStyleSheet(pinned
            ? "QPushButton { background: rgba(99,102,241,0.4); border: none; border-radius: 14px; "
              "font-size: 14px; }"
              "QPushButton:hover { background: rgba(99,102,241,0.6); }"
            : "QPushButton { background: rgba(255,255,255,0.1); border: none; border-radius: 14px; "
              "font-size: 14px; }"
              "QPushButton:hover { background: rgba(255,255,255,0.2); }");
        pinBtn_->setToolTip(pinned
            ? QString::fromUtf8("取消置顶")
            : QString::fromUtf8("置顶"));
    }
}
