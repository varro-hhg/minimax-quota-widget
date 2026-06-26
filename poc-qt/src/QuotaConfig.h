// poc-qt/src/QuotaConfig.h
// 配置管理 — MiniMax / 火山方舟凭证存储
// 路径优先级: ~/.mmx/config.json > ~/.minimax-quota-widget/config.json

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QuotaConfig : public QObject
{
    Q_OBJECT

public:
    explicit QuotaConfig(QObject* parent = nullptr);

    // 加载所有 provider 配置
    // 返回 {minimax: {...}|null, volcengine: {...}|null}
    QJsonValue minimax() const { return minimax_; }
    QJsonValue volcengine() const { return volcengine_; }
    QString configPath() const { return configPath_; }
    bool wasCreated() const { return wasCreated_; }

    // 确保配置文件存在，不存在则创建示例
    void ensureLoaded();

    // 打开配置文件所在目录
    void openConfigFile() const;
    void openConfigDir() const;

    // 重新加载（用户编辑后点刷新）
    void reload();

    // 颜色阈值辅助
    static QString colorFor(int percent);

private:
    void loadInternal();
    bool migrateOldFormat(const QString& path, QJsonObject& cfg);
    void createExampleConfig(const QString& path);

    QJsonValue minimax_;
    QJsonValue volcengine_;
    QString     configPath_;
    bool        wasCreated_ = false;
};
