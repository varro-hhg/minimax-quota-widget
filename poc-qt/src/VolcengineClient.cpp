// poc-qt/src/VolcengineClient.cpp
// Volcengine Coding Plan quota client
// Uses QProcess + curl to bypass Qt SSL stack issues

#include "VolcengineClient.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QUrl>
#include <QMap>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <cmath>

namespace {

// ============================================================
// Constants
// ============================================================

const char* kService  = "ark";
const char* kRegion   = "cn-beijing";
const char* kHost     = "open.volcengineapi.com";
const char* kEndpoint = "https://open.volcengineapi.com";
const char* kAlgorithm = "HMAC-SHA256";

// ============================================================
// Low-level crypto helpers
// ============================================================

QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QByteArray sha256Hex(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QString toXDate(const QDateTime& dt)
{
    QDateTime u = dt.toUTC();
    return QString("%1%2%3T%4%5%6Z")
        .arg(u.date().year(), 4, 10, QChar('0'))
        .arg(u.date().month(), 2, 10, QChar('0'))
        .arg(u.date().day(), 2, 10, QChar('0'))
        .arg(u.time().hour(), 2, 10, QChar('0'))
        .arg(u.time().minute(), 2, 10, QChar('0'))
        .arg(u.time().second(), 2, 10, QChar('0'));
}

QString toDateStamp(const QDateTime& dt)
{
    return toXDate(dt).left(8);
}

// ============================================================
// V4 signing
// ============================================================

QMap<QByteArray, QByteArray> signRequest(
    const QString& accessKeyId,
    const QString& secretAccessKey,
    const QDateTime& now,
    const QByteArray& body)
{
    const QByteArray xDate     = toXDate(now).toUtf8();
    const QByteArray dateStamp = toDateStamp(now).toUtf8();
    const QByteArray bodyHash  = sha256Hex(body);

    QMap<QByteArray, QByteArray> headers;
    headers["Content-Type"]   = "application/json";
    headers["X-Date"]         = xDate;
    headers["X-Content-Sha256"] = bodyHash;
    headers["Host"]           = QByteArray(kHost);

    // Canonical headers: lowercase keys, sorted
    QStringList lowerKeys;
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        lowerKeys.append(QString::fromUtf8(it.key()).toLower());
    }
    std::sort(lowerKeys.begin(), lowerKeys.end());

    QString canonicalHeaders;
    for (const QString& lk : lowerKeys) {
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
            if (QString::fromUtf8(it.key()).toLower() == lk) {
                canonicalHeaders += lk + ":" + QString::fromUtf8(it.value()).trimmed() + "\n";
                break;
            }
        }
    }
    const QByteArray signedHeaders = lowerKeys.join(";").toUtf8();

    const QByteArray canonicalURI = "/";
    // Canonical query string: 必须包含 URL 中的查询参数，按 key 排序，URI 编码
    // Action=GetCodingPlanUsage&Version=2024-01-01 (已按字母序)
    const QByteArray canonicalQuery = "Action=GetCodingPlanUsage&Version=2024-01-01";
    const QByteArray canonicalRequest = QByteArray("POST\n") + canonicalURI + "\n"
        + canonicalQuery + "\n" + canonicalHeaders.toUtf8() + "\n" + signedHeaders + "\n" + bodyHash;

    const QByteArray credentialScope = dateStamp + "/" + QByteArray(kRegion) + "/" + QByteArray(kService) + "/request";
    const QByteArray stringToSign = QByteArray(kAlgorithm) + "\n" + xDate + "\n"
        + credentialScope + "\n" + sha256Hex(canonicalRequest);

    const QByteArray sk      = secretAccessKey.toUtf8();
    const QByteArray kDate   = hmacSha256(sk, dateStamp);
    const QByteArray kRegion = hmacSha256(kDate, QByteArray(::kRegion));
    const QByteArray kService = hmacSha256(kRegion, QByteArray(::kService));
    const QByteArray kSigning = hmacSha256(kService, QByteArray("request"));

    const QByteArray signature = hmacSha256(kSigning, stringToSign).toHex();

    const QByteArray authorization = QByteArray(kAlgorithm) + " Credential=" + accessKeyId.toUtf8() + "/" + credentialScope
        + ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;

    headers["Authorization"] = authorization;
    return headers;
}

// ============================================================
// Response normalization
// ============================================================

const QMap<QString, QString> kLevelLabels = {
    {"session", QString::fromUtf8("会话")},
    {"weekly",  QString::fromUtf8("本周")},
    {"monthly", QString::fromUtf8("本月")}
};

const QStringList kLevelOrder = {"session", "weekly", "monthly"};

VolcQuota normalize(const QJsonObject& q)
{
    VolcQuota out;
    QString level = q.value("Level").toString().toLower();
    out.level        = level.isEmpty() ? "unknown" : level;
    out.label        = kLevelLabels.value(out.level, q.value("Level").toString());
    double pct       = q.value("Percent").toDouble();
    out.usedPercent  = std::round(pct * 100.0) / 100.0;
    out.remainPercent = std::round((100.0 - pct) * 100.0) / 100.0;
    if (out.remainPercent < 0) out.remainPercent = 0;
    qint64 ts        = q.value("ResetTimestamp").toVariant().toLongLong();
    out.resetsAt     = ts * 1000;  // seconds -> ms
    if (out.resetsAt > 0) {
        out.resetsInMs = std::max<qint64>(0, out.resetsAt - QDateTime::currentMSecsSinceEpoch());
    }
    return out;
}

}  // namespace

VolcengineClient::VolcengineClient(QObject* parent)
    : QObject(parent)
{
}

VolcengineClient::~VolcengineClient() = default;

void VolcengineClient::fetch(const QJsonObject& creds, int timeoutMs)
{
    QString ak = creds.value("accessKeyId").toString();
    QString sk = creds.value("secretAccessKey").toString();
    if (ak.isEmpty() || sk.isEmpty()) {
        emit usageError(QString::fromUtf8("Volcengine AK/SK missing"));
        return;
    }

    // Compute V4 signature
    QByteArray body = "{}";
    QDateTime now = QDateTime::currentDateTimeUtc();
    QMap<QByteArray, QByteArray> headers = signRequest(ak, sk, now, body);

    // Build URL
    QUrl url(QString("%1/?Action=GetCodingPlanUsage&Version=2024-01-01").arg(kEndpoint));

    // Build curl args
    QStringList args;
    args << "-s"
         << "--max-time" << QString::number(timeoutMs / 1000)
         << "-X" << "POST"
         << "-H" << "Content-Type: application/json"
         << "-H" << QString("Authorization: %1").arg(QString::fromUtf8(headers.value("Authorization")))
         << "-H" << QString("X-Date: %1").arg(QString::fromUtf8(headers.value("X-Date")))
         << "-H" << QString("X-Content-Sha256: %1").arg(QString::fromUtf8(headers.value("X-Content-Sha256")))
         << "-H" << QString("Host: %1").arg(kHost)
         << "-d" << "{}"
         << url.toString();

    qDebug() << "[volc] POST" << url.toString();

    // Synchronous QProcess + QEventLoop
    QProcess proc(this);
    proc.start("curl", args);

    QEventLoop loop;
    QObject::connect(&proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     &loop, &QEventLoop::quit);
    QTimer killTimer;
    killTimer.setSingleShot(true);
    QObject::connect(&killTimer, &QTimer::timeout, [&]() {
        qDebug() << "[volc] timeout, killing curl";
        proc.kill();
    });
    killTimer.start(timeoutMs);
    loop.exec();

    qDebug() << "[volc] curl exit=" << proc.exitCode();

    if (proc.exitStatus() == QProcess::CrashExit) {
        emit usageError(QString::fromUtf8("Volcengine: curl crashed"));
        return;
    }

    QByteArray text = proc.readAllStandardOutput();
    qDebug() << "[volc] body_len=" << text.length();
    if (proc.exitCode() != 0) {
        QByteArray err = proc.readAllStandardError();
        emit usageError(QString::fromUtf8("Volc HTTP error: %1").arg(QString::fromUtf8(err.left(300))));
        return;
    }

    // Parse JSON response
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit usageError(QString::fromUtf8("Volc: invalid JSON: %1").arg(QString::fromUtf8(text.left(200))));
        return;
    }

    QJsonObject root = doc.object();
    if (root.value("ResponseMetadata").toObject().value("Error").isObject()) {
        QJsonObject e = root.value("ResponseMetadata").toObject().value("Error").toObject();
        QString code = e.value("Code").toString();
        if (code.isEmpty()) code = "VolcError";
        QString msg  = e.value("Message").toString();
        if (msg.isEmpty()) msg = QString(QJsonDocument(e).toJson(QJsonDocument::Compact));
        qDebug() << "[volc] API error:" << code << msg;
        emit usageError(QString::fromUtf8("%1: %2").arg(code, msg));
        return;
    }

    QJsonObject result = root.value("Result").toObject();
    QJsonArray arr     = result.value("QuotaUsage").toArray();

    VolcUsage usage;
    usage.status = result.value("Status").toString();
    if (usage.status.isEmpty()) usage.status = "Unknown";
    usage.quotas.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (v.isObject()) usage.quotas.append(normalize(v.toObject()));
    }
    std::sort(usage.quotas.begin(), usage.quotas.end(),
              [](const VolcQuota& a, const VolcQuota& b) {
                  int ia = kLevelOrder.indexOf(a.level);
                  int ib = kLevelOrder.indexOf(b.level);
                  if (ia < 0) ia = 99;
                  if (ib < 0) ib = 99;
                  return ia < ib;
              });

    qint64 upd = result.value("UpdateTimestamp").toVariant().toLongLong();
    usage.updatedAt = upd > 0
        ? QDateTime::fromMSecsSinceEpoch(upd * 1000).toString(Qt::ISODate)
        : QDateTime::currentDateTime().toString(Qt::ISODate);

    emit usageReady(usage);
}
