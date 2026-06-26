// poc-qt/src/MainWindow.cpp
// 主窗口：装配 QuotaView / Clients / Tray / Timers

#include "MainWindow.h"

#include "MiniMaxClient.h"
#include "QuotaConfig.h"
#include "QuotaView.h"
#include "TrayIcon.h"
#include "VolcengineClient.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kWidth        = 460;
constexpr int kHeight       = 580;
constexpr int kCornerRadius = 18;
}

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QString::fromUtf8("Coding Plan 配额"));
    setWindowIcon(QIcon(":/icon.png"));
    setAttribute(Qt::WA_TranslucentBackground);
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
    if (isPinned_) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setFixedSize(kWidth, kHeight);  // 固定大小，不可拉伸

    // 屏幕右上角
    if (auto* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - kWidth - 24, avail.top() + 24);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Logic ──
    config_   = new QuotaConfig(this);
    view_     = new QuotaView(this);
    minimax_  = new MiniMaxClient(this);
    volc_     = new VolcengineClient(this);
    tray_     = new TrayIcon(this);

    view_->setConfigPath(config_->configPath(), config_->wasCreated());
    layout->addWidget(view_);

    connect(view_, &QuotaView::closeRequested, this, &MainWindow::onCloseClicked);
    connect(view_, &QuotaView::pinToggled,     this, &MainWindow::togglePin);
    connect(view_, &QuotaView::openConfigRequested, this, [this]() {
        if (config_) config_->openConfigFile();
    });
    // 同步初始置顶状态到 view
    if (view_) view_->setPinned(isPinned_);

    connect(minimax_, &MiniMaxClient::quotaReady,  this, &MainWindow::onMiniMaxReady);
    connect(minimax_, &MiniMaxClient::quotaError,  this, &MainWindow::onMiniMaxError);
    connect(volc_,    &VolcengineClient::usageReady, this, &MainWindow::onVolcReady);
    connect(volc_,    &VolcengineClient::usageError, this, &MainWindow::onVolcError);

    // ── Tray wiring ──
    connect(tray_, &TrayIcon::showRequested,       this, &MainWindow::onTrayShow);
    connect(tray_, &TrayIcon::refreshRequested,    this, &MainWindow::onTrayRefresh);
    connect(tray_, &TrayIcon::togglePinRequested,  this, &MainWindow::onTrayTogglePin);
    connect(tray_, &TrayIcon::openConfigRequested, this, &MainWindow::onTrayOpenConfig);
    connect(tray_, &TrayIcon::quitRequested,       this, &MainWindow::onTrayQuit);

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_->setVisible(true);
    } else {
        qWarning() << "[main] System tray not available";
    }

    // ── Timers ──
    countdownTimer_ = new QTimer(this);
    countdownTimer_->setInterval(30 * 1000);
    connect(countdownTimer_, &QTimer::timeout, this, &MainWindow::onCountdownTick);
    countdownTimer_->start();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setSingleShot(true);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshQuota);

    // 启动后 1 秒拉取一次
    QTimer::singleShot(1000, this, &MainWindow::refreshQuota);
}

MainWindow::~MainWindow() = default;

// ── Refresh flow ──

void MainWindow::refreshQuota()
{
    qDebug() << "[main] refreshQuota fired";
    if (!view_) return;
    view_->setStatus(QString::fromUtf8("正在刷新…"), true, true);

    QJsonValue mm = config_->minimax();
    QJsonValue vv = config_->volcengine();

    bool hasMiniMax = mm.isObject();
    bool hasVolc    = vv.isObject();

    if (!hasMiniMax && !hasVolc) {
        view_->setError(QString::fromUtf8("未配置 — 请编辑 API Key 后刷新"),
                        /*isNoConfig=*/true);
        scheduleNextRefresh(-1);
        return;
    }

    if (hasMiniMax) {
        minimax_->fetch(mm.toObject(), 10'000);
    } else {
        onMiniMaxError(QString::fromUtf8("MiniMax not configured"));
    }

    if (hasVolc) {
        volc_->fetch(vv.toObject(), 10'000);
    } else {
        onVolcError(QString::fromUtf8("NO_VOLC_CONFIG"));
    }
}

void MainWindow::onMiniMaxReady(QList<MiniMaxModel> models)
{
    view_->setMiniMaxData(models);
    view_->setStatus(
        QString::fromUtf8("在线 | 上次刷新 %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")),
        true);

    qint64 next = 0;
    for (const auto& m : models) {
        if (m.modelName == "general") { next = m.intervalResetsInMs; break; }
    }
    scheduleNextRefresh(next);
}

void MainWindow::onMiniMaxError(QString msg)
{
    view_->setError(msg, msg == QString::fromUtf8("MiniMax API key missing") || msg == QString::fromUtf8("未配置"));
    scheduleNextRefresh(-1);
}

void MainWindow::onVolcReady(VolcUsage usage)
{
    view_->setVolcData(usage);
    view_->setStatus(
        QString::fromUtf8("在线 | 上次刷新 %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")),
        true);
}

void MainWindow::onVolcError(QString msg)
{
    if (msg == QString::fromUtf8("NO_VOLC_CONFIG")) {
        view_->setVolcData(VolcUsage{});  // 留空
        return;
    }
    view_->setError(msg, false);
}

void MainWindow::onCountdownTick()
{
    if (view_) view_->tickCountdown();
}

// ── Tray handlers ──

void MainWindow::onTrayShow()
{
    if (isVisible()) hide();
    else { show(); raise(); activateWindow(); }
}

void MainWindow::onTrayRefresh()
{
    refreshQuota();
}

void MainWindow::onTrayTogglePin()
{
    if (tray_) togglePin();
}

void MainWindow::onTrayOpenConfig()
{
    if (config_) config_->openConfigFile();
}

void MainWindow::onTrayQuit()
{
    isQuitting_ = true;
    qApp->quit();
}

void MainWindow::togglePin()
{
    isPinned_ = !isPinned_;
    setWindowFlag(Qt::WindowStaysOnTopHint, isPinned_);
    // setWindowFlag 需要重建原生窗口才能生效，hide+show 强制重建
    hide();
    show();
    if (tray_) tray_->setPinned(isPinned_);
    if (view_) view_->setPinned(isPinned_);
}

void MainWindow::onCloseClicked()
{
    hide();
}

// ── Painting / drag ──

void MainWindow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), kCornerRadius, kCornerRadius);
    p.fillPath(path, QColor(18, 18, 28, 230));
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_     = true;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        dragStartPos_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
#else
        dragStartPos_ = event->globalPos() - frameGeometry().topLeft();
#endif
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        move(event->globalPosition().toPoint() - dragStartPos_);
#else
        move(event->globalPos() - dragStartPos_);
#endif
        event->accept();
    }
}

void MainWindow::leaveEvent(QEvent* /*event*/)
{
    dragging_ = false;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!isQuitting_) {
        // Hide to tray instead of quit
        event->ignore();
        hide();
    } else {
        event->accept();
    }
}

void MainWindow::scheduleNextRefresh(qint64 generalResetMs)
{
    if (!refreshTimer_) return;
    refreshTimer_->stop();
    // 如果 general reset 在 2 分钟内，5 秒后刷新；否则 5 分钟
    int ms = (generalResetMs > 0 && generalResetMs < 120'000) ? 5'000 : 5 * 60'000;
    refreshTimer_->start(ms);
}
