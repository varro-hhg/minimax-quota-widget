// poc-qt/src/VolcengineClient.h
// 火山方舟 Coding Plan quota V4-signed HTTP 客户端
// Endpoint: POST https://open.volcengineapi.com/?Action=GetCodingPlanUsage&Version=2024-01-01
// Implementation: QProcess + curl (bypass Qt SSL issues)

#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct VolcQuota
{
    QString level;
    QString label;
    double  usedPercent   = 0;
    double  remainPercent = 0;
    qint64  resetsAt      = 0;
    qint64  resetsInMs    = 0;
};

struct VolcUsage
{
    QString          status;
    QList<VolcQuota> quotas;
    QString          updatedAt;
};

class QProcess;

class VolcengineClient : public QObject
{
    Q_OBJECT

public:
    explicit VolcengineClient(QObject* parent = nullptr);
    ~VolcengineClient() override;

    // creds: {accessKeyId, secretAccessKey}
    void fetch(const QJsonObject& creds, int timeoutMs = 10000);

signals:
    void usageReady(VolcUsage usage);
    void usageError(QString message);
};
