// poc-qt/src/MainWindow.h
// 透明液态玻璃风格的主窗口（仅 PoC：先验证 UI 框架，后续接入 API）

#pragma once

#include <QWidget>

class QLabel;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QPoint dragStartPos_;
    bool   dragging_ = false;
    QLabel* statusLabel_ = nullptr;
};
