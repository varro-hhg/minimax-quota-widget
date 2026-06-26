// poc-qt/src/TrayIcon.cpp

#include "TrayIcon.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace {

QIcon makeFallbackIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0x33, 0x70, 0xFF));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(pm.rect().adjusted(4, 4, -4, -4), 12, 12);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(28);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, QString::fromUtf8("M"));
    return QIcon(pm);
}

}  // namespace

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
    , icon_(new QSystemTrayIcon(this))
    , menu_(new QMenu())
{
    icon_->setIcon(makeFallbackIcon());
    icon_->setToolTip(QString::fromUtf8("Coding Plan 配额"));

    auto* showHide = menu_->addAction(QString::fromUtf8("显示 / 隐藏"));
    auto* refresh  = menu_->addAction(QString::fromUtf8("刷新额度"));
    pinAction_     = menu_->addAction(QString::fromUtf8("取消置顶"));
    pinAction_->setCheckable(true);
    pinAction_->setChecked(true);
    menu_->addSeparator();
    auto* cfg = menu_->addAction(QString::fromUtf8("📂 打开配置文件"));
    menu_->addSeparator();
    auto* quit = menu_->addAction(QString::fromUtf8("退出"));

    connect(showHide,  &QAction::triggered, this, &TrayIcon::showRequested);
    connect(refresh,   &QAction::triggered, this, &TrayIcon::refreshRequested);
    connect(pinAction_,&QAction::triggered, this, &TrayIcon::togglePinRequested);
    connect(cfg,       &QAction::triggered, this, &TrayIcon::openConfigRequested);
    connect(quit,      &QAction::triggered, this, &TrayIcon::quitRequested);

    icon_->setContextMenu(menu_);
    connect(icon_, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

TrayIcon::~TrayIcon()
{
    delete menu_;
}

void TrayIcon::setVisible(bool v)
{
    visible_ = v;
    icon_->setVisible(v);
}

void TrayIcon::setPinned(bool pinned)
{
    pinned_ = pinned;
    pinAction_->blockSignals(true);
    pinAction_->setChecked(pinned);
    pinAction_->setText(pinned ? QString::fromUtf8("取消置顶") : QString::fromUtf8("置顶"));
    pinAction_->blockSignals(false);
}

void TrayIcon::refreshMenu()
{
    menu_->update();
}

void TrayIcon::onActivated(int reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        emit showRequested();
    }
}
