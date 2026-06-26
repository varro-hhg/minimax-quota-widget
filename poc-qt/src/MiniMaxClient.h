// poc-qt/src/MiniMaxClient.h
// MiniMax Token Plan quota HTTP 客户端
// Endpoint: GET {baseUrl}/v1/token_plan/remains
// Implementation: QProcess + curl (bypass Qt SSL issues)

#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct MiniMaxModel
{
    QString modelName;
    int     intervalRemainPercent = 100;
    int     weeklyRemainPercent   = 100;
    qint64  intervalUsed = 0;
    qint64  intervalTotal = 0;
    qint64  weeklyUsed   = 0;
    qint64  weeklyTotal  = 0;
    qint64  intervalResetsInMs = 0;
    int     intervalBoostPermille = 0;
    int     weeklyBoostPermille   = 0;
    QString intervalStatus;
    QString weeklyStatus;
    qint64  weeklyStart = 0;
    qint64  weeklyEnd   = 0;
    QString fetchedAt;
};

class QProcess;
class QTimer;

class MiniMaxClient : public QObject
{
    Q_OBJECT

public:
    explicit MiniMaxClient(QObject* parent = nullptr);
    ~MiniMaxClient() override;

    // cfg: {apiKey, region, baseUrl}
    void fetch(const QJsonObject& cfg, int timeoutMs = 10000);

signals:
    void quotaReady(QList<MiniMaxModel> models);
    void quotaError(QString message);

private:
    // QProcess-based implementation (no QNetworkAccessManager needed)
};
