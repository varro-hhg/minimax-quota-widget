// poc-qt/src/MainWindow.cpp
// PoC: 透明圆角窗口 + 液态玻璃背景 + 可拖动标题栏

#include "MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>

namespace {
constexpr int kWidth        = 460;
constexpr int kHeight       = 500;
constexpr int kCornerRadius = 18;
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Coding Plan 配额"));

    // 透明 + 无边框 + 工具窗口（不在任务栏出现）
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

    resize(kWidth, kHeight);

    // 放到屏幕右上角
    if (auto* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - kWidth - 24, avail.top() + 24);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("CODING PLAN 配额"), this);
    title->setStyleSheet(
        "color: rgba(255,255,255,0.75);"
        "font-size: 12px;"
        "font-weight: 700;"
        "letter-spacing: 2px;");
    layout->addWidget(title);

    statusLabel_ = new QLabel(
        QStringLiteral("✅ Qt 5.13.1 PoC 启动成功\n\n"
                       "下一步：\n"
                       "  • 接入 MiniMax / 火山方舟 API\n"
                       "  • 配置文件管理\n"
                       "  • 系统托盘\n"
                       "  • 进度条 UI"),
        this);
    statusLabel_->setStyleSheet(
        "color: rgba(255,255,255,0.85);"
        "font-size: 12px;"
        "line-height: 1.6;");
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);
    layout->addStretch();
}

void MainWindow::paintEvent(QPaintEvent* /*event*/)
{
    // 圆角玻璃背景（半透明深色 + 边框）
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), kCornerRadius, kCornerRadius);

    // 主体背景
    p.fillPath(path, QColor(18, 18, 28, 230));

    // 1px 高光边框
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
