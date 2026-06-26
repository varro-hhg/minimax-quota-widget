// poc-qt/src/ProgressBar.h
// 自绘进度条：圆角 track + 颜色梯度 fill（绿/黄/红）

#pragma once

#include <QString>
#include <QWidget>

class ProgressBar : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressBar(QWidget* parent = nullptr);

    void setPercent(int pct);
    void setBoostLabel(const QString& label);

    int  percent() const { return percent_; }
    void setResetLabel(const QString& label);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int     percent_    = 100;
    QString boostLabel_;
    QString resetLabel_;
};
