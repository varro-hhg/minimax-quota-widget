// poc-qt/src/MiniMaxClient.cpp
// MiniMax Token Plan quota HTTP 客户端
// Endpoint: GET {baseUrl}/v1/token_plan/remains
// Implementation: QProcess + curl (bypass Qt SSL issues on this machine)

#include "MiniMaxClient.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace {

int clampPercent(QJsonValue v)
{
    if (v.isDouble()) {
        double d = v.toDouble();
        if (!std::isfinite(d)) return 0;
        int i = static_cast<int>(std::round(d));
        return std::max(0, std::min(100, i));
    }
    if (v.isString()) {
        bool ok = false;
        double d = v.toString().toDouble(&ok);
        if (!ok || !std::isfinite(d)) return 100;
        int i = static_cast<int>(std::round(d));
        return std::max(0, std::min(100, i));
    }
    return 100;
}

qint64 toInt64(QJsonValue v)
{
    if (v.isDouble()) return v.toVariant().toLongLong();
    if (v.isString()) return v.toString().toLongLong();
    return 0;
}

QString toStr(QJsonValue v, const QString& def = QString())
{
    return v.isString() ? v.toString() : def;
}

MiniMaxModel normalize(const QJsonObject& m)
{
    MiniMaxModel out;
    out.modelName             = toStr(m.value("model_name"), "unknown");
    out.intervalRemainPercent = clampPercent(m.value("current_interval_remaining_percent"));
    out.weeklyRemainPercent   = clampPercent(m.value("current_weekly_remaining_percent"));
    out.intervalUsed          = toInt64(m.value("current_interval_usage_count"));
    out.intervalTotal         = toInt64(m.value("current_interval_total_count"));
    out.weeklyUsed            = toInt64(m.value("current_weekly_usage_count"));
    out.weeklyTotal           = toInt64(m.value("current_weekly_total_count"));
    out.intervalResetsInMs    = toInt64(m.value("remains_time"));
    out.intervalBoostPermille = static_cast<int>(toInt64(m.value("interval_boost_permille")));
    out.weeklyBoostPermille   = static_cast<int>(toInt64(m.value("weekly_boost_permille")));
    out.intervalStatus        = toStr(m.value("current_interval_status"));
    out.weeklyStatus          = toStr(m.value("current_weekly_status"));
    out.weeklyStart           = toInt64(m.value("weekly_start_time"));
    out.weeklyEnd             = toInt64(m.value("weekly_end_time"));
    out.fetchedAt             = QDateTime::currentDateTime().toString(Qt::ISODate);
    return out;
}

}  // anonymous namespace

MiniMaxClient::MiniMaxClient(QObject* parent)
    : QObject(parent)
{
}

MiniMaxClient::~MiniMaxClient() = default;

void MiniMaxClient::fetch(const QJsonObject& cfg, int timeoutMs)
{
    QString apiKey  = cfg.value("apiKey").toString();
    QString baseUrl = cfg.value("baseUrl").toString();
    if (baseUrl.isEmpty()) baseUrl = "https://api.minimaxi.com";
    if (apiKey.isEmpty()) {
        emit quotaError(QString::fromUtf8("MiniMax API key missing"));
        return;
    }

    QString url = baseUrl + "/v1/token_plan/remains";
    qDebug() << "[minimax] GET" << url;

    // Build curl args: -s silent, --max-time, headers, URL
    QStringList args;
    args << "-s"
         << "--max-time" << QString::number(timeoutMs / 1000)
         << "-H" << QString("Authorization: Bearer %1").arg(apiKey)
         << "-H" << QString("x-api-key: %1").arg(apiKey)
         << "-H" << "Content-Type: application/json"
         << url;

    // Synchronous QProcess + QEventLoop (avoids Qt SSL stack issues)
    QProcess proc(this);
    proc.start("curl", args);
    qDebug() << "[minimax] curl pid=" << proc.processId();

    QEventLoop loop;
    QObject::connect(&proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     &loop, &QEventLoop::quit);
    QTimer killTimer;
    killTimer.setSingleShot(true);
    QObject::connect(&killTimer, &QTimer::timeout, [&]() {
        qDebug() << "[minimax] timeout, killing curl";
        proc.kill();
    });
    killTimer.start(timeoutMs);
    loop.exec();

    if (proc.exitStatus() == QProcess::CrashExit) {
        emit quotaError(QString::fromUtf8("MiniMax: curl crashed"));
        return;
    }

    QByteArray body = proc.readAllStandardOutput();
    qDebug() << "[minimax] curl exit=" << proc.exitCode()
             << "body_len=" << body.length();

    if (proc.exitCode() != 0) {
        QByteArray err = proc.readAllStandardError();
        emit quotaError(QString::fromUtf8("MiniMax HTTP error: %1")
                            .arg(QString::fromUtf8(err.left(200))));
        return;
    }

    // Parse JSON response
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit quotaError(QString::fromUtf8("MiniMax: invalid JSON response"));
        return;
    }

    QJsonObject root = doc.object();
    QJsonValue br = root.value("base_resp");
    if (br.isObject()) {
        int code = br.toObject().value("status_code").toInt(-1);
        if (code != 0) {
            QString m = br.toObject().value("status_msg").toString();
            if (m.isEmpty()) m = "MiniMax API error";
            emit quotaError(m);
            return;
        }
    }

    QList<MiniMaxModel> models;
    QJsonArray arr = root.value("model_remains").toArray();
    models.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (v.isObject()) models.append(normalize(v.toObject()));
    }
    emit quotaReady(models);
}
