// poc-qt/src/ProgressBar.cpp

#include "ProgressBar.h"
#include "QuotaConfig.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

ProgressBar::ProgressBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ProgressBar::setPercent(int pct)
{
    percent_ = std::clamp(pct, 0, 100);
    update();
}

void ProgressBar::setBoostLabel(const QString& label)
{
    boostLabel_ = label;
    update();
}

void ProgressBar::setResetLabel(const QString& label)
{
    resetLabel_ = label;
    update();
}

static QColor colorForPercent(int pct)
{
    if (pct <= 0 || pct < 10) return QColor(0xf8, 0x71, 0x71);  // red
    if (pct < 50)             return QColor(0xfa, 0xcc, 0x15);  // yellow
    return QColor(0x4a, 0xde, 0x80);                            // green
}

void ProgressBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int barH   = 6;
    const int barY   = (height() - barH) / 2;
    const int trackX = 0;
    const int barW   = width() - 100;  // 留 100px 给标签

    // Track (圆角半透明底)
    QPainterPath track;
    track.addRoundedRect(QRectF(trackX, barY, barW, barH), 3, 3);
    p.fillPath(track, QColor(255, 255, 255, 25));

    // Fill
    if (percent_ > 0) {
        int fillW = std::max(1, barW * percent_ / 100);
        QPainterPath fill;
        fill.addRoundedRect(QRectF(trackX, barY, fillW, barH), 3, 3);

        QColor c = colorForPercent(percent_);
        // 简单渐变（从浅到深）
        QLinearGradient g(QPointF(trackX, barY), QPointF(trackX, barY + barH));
        g.setColorAt(0, c.lighter(115));
        g.setColorAt(1, c.darker(115));
        p.fillPath(fill, g);
    }

    // Boost tag (右上)
    if (!boostLabel_.isEmpty()) {
        QFont f = p.font();
        f.setPointSizeF(10.5);
        f.setBold(true);
        p.setFont(f);

        QFontMetrics fm(f);
        int bw = fm.horizontalAdvance(boostLabel_) + 10;
        int bh = 14;
        int bx = width() - bw - (resetLabel_.isEmpty() ? 0 : 50);
        int by = (height() - bh) / 2;

        QPainterPath bp;
        bp.addRoundedRect(QRectF(bx, by, bw, bh), 7, 7);
        p.fillPath(bp, QColor(99, 102, 241, 90));
        p.setPen(QColor(165, 165, 255));
        p.drawText(QRectF(bx, by, bw, bh), Qt::AlignCenter, boostLabel_);
    }

    // Reset tag (右下)
    if (!resetLabel_.isEmpty()) {
        QFont f = p.font();
        f.setPointSizeF(10.5);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 100));
        QFontMetrics fm(f);
        int rw = fm.horizontalAdvance(resetLabel_) + 4;
        QRectF r(width() - rw, (height() - fm.height()) / 2, rw, fm.height());
        p.drawText(r, Qt::AlignVCenter | Qt::AlignRight, resetLabel_);
    }
}
