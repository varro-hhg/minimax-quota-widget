// poc-qt/src/QuotaConfig.cpp

#include "QuotaConfig.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QDesktopServices>

namespace {

constexpr const char* kPlaceholderMinimaxKey = "sk-your-minimax-api-key-here";
constexpr const char* kPlaceholderVolcAk     = "your-volcengine-access-key-id";
constexpr const char* kPlaceholderVolcSk     = "your-volcengine-secret-access-key";

QString widgetConfigPath()
{
    return QDir::toNativeSeparators(
        QDir::homePath() + "/.minimax-quota-widget/config.json");
}

QString mmxConfigPath()
{
    return QDir::toNativeSeparators(
        QDir::homePath() + "/.mmx/config.json");
}

bool fileExists(const QString& path)
{
    return QFileInfo::exists(path);
}

QString readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

bool writeFile(const QString& path, const QString& content)
{
    QFileInfo fi(path);
    QDir dir  = fi.dir();
    if (!dir.exists() && !dir.mkpath(".")) return false;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content.toUtf8());
    return true;
}

QString exampleConfig()
{
    QJsonObject providers;

    QJsonObject minimax;
    minimax["enabled"] = true;
    minimax["api_key"] = QString::fromUtf8(kPlaceholderMinimaxKey);
    minimax["region"]  = "cn";
    providers["minimax"] = minimax;

    QJsonObject volc;
    volc["enabled"]          = true;
    volc["access_key_id"]    = QString::fromUtf8(kPlaceholderVolcAk);
    volc["secret_access_key"] = QString::fromUtf8(kPlaceholderVolcSk);
    providers["volcengine"] = volc;

    QJsonObject root;
    root["providers"] = providers;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

}  // namespace

QuotaConfig::QuotaConfig(QObject* parent)
    : QObject(parent)
{
    ensureLoaded();
}

void QuotaConfig::ensureLoaded()
{
    // 1. mmx-cli 优先
    if (fileExists(mmxConfigPath())) {
        configPath_ = mmxConfigPath();
        loadInternal();
        return;
    }

    // 2. widget config
    if (fileExists(widgetConfigPath())) {
        configPath_ = widgetConfigPath();
        loadInternal();
        return;
    }

    // 3. 都不存在 → 创建示例
    QString p = widgetConfigPath();
    createExampleConfig(p);
    configPath_ = p;
    wasCreated_ = true;
    loadInternal();
}

void QuotaConfig::reload()
{
    minimax_    = QJsonValue();
    volcengine_ = QJsonValue();
    wasCreated_ = false;
    ensureLoaded();
}

void QuotaConfig::loadInternal()
{
    if (configPath_.isEmpty()) return;

    // 旧格式 → 新格式迁移
    QJsonObject cfg;
    {
        QString raw = readFile(configPath_);
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (!doc.isObject()) return;
        cfg = doc.object();
    }
    if (migrateOldFormat(configPath_, cfg)) {
        // 重新读取迁移后的文件
        QString raw = readFile(configPath_);
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isObject()) cfg = doc.object();
    }

    QJsonObject providers = cfg.value("providers").toObject();

    // minimax
    QJsonValue m = providers.value("minimax");
    if (m.isObject() && m.toObject().value("enabled").toBool(true)) {
        QString key = m.toObject().value("api_key").toString();
        if (!key.isEmpty() && key != QString::fromUtf8(kPlaceholderMinimaxKey)) {
            QString region = m.toObject().value("region").toString();
            if (region.isEmpty()) region = "cn";
            QJsonObject o;
            o["apiKey"]  = key;
            o["region"]  = region;
            o["baseUrl"] = (region == "global")
                ? "https://api.minimax.io"
                : "https://api.minimaxi.com";
            minimax_ = o;
        }
    }

    // volcengine
    QJsonValue v = providers.value("volcengine");
    if (v.isObject() && v.toObject().value("enabled").toBool(true)) {
        QJsonObject vc = v.toObject();
        QString ak = vc.value("access_key_id").toString();
        QString sk = vc.value("secret_access_key").toString();
        if (!ak.isEmpty() && !sk.isEmpty()
            && ak != QString::fromUtf8(kPlaceholderVolcAk)
            && sk != QString::fromUtf8(kPlaceholderVolcSk)) {
            QJsonObject o;
            o["accessKeyId"]     = ak;
            o["secretAccessKey"] = sk;
            volcengine_ = o;
        }
    }
}

bool QuotaConfig::migrateOldFormat(const QString& path, QJsonObject& cfg)
{
    if (cfg.value("providers").isObject()) return false;

    bool hasOldMinimax = cfg.contains("api_key") || cfg.contains("apiKey");
    bool hasOldVolc    = cfg.contains("volcengine") || cfg.contains("volc");
    if (!hasOldMinimax && !hasOldVolc) return false;

    qInfo() << "[config] Detected old format, migrating to providers format...";

    QJsonObject providers;

    if (hasOldMinimax) {
        QString key = cfg.value("apiKey").toString();
        if (key.isEmpty()) key = cfg.value("api_key").toString();
        QJsonObject m;
        m["enabled"] = true;
        m["api_key"] = key;
        m["region"]  = cfg.value("region").toString();
        if (m["region"].toString().isEmpty()) m["region"] = "cn";
        providers["minimax"] = m;
    }

    if (hasOldVolc) {
        QJsonObject vc = cfg.value("volcengine").toObject();
        if (vc.isEmpty()) vc = cfg.value("volc").toObject();
        QString ak = vc.value("access_key_id").toString();
        if (ak.isEmpty()) ak = vc.value("accessKeyId").toString();
        QString sk = vc.value("secret_access_key").toString();
        if (sk.isEmpty()) sk = vc.value("secretAccessKey").toString();
        if (!ak.isEmpty() && !sk.isEmpty()) {
            QJsonObject v;
            v["enabled"]           = true;
            v["access_key_id"]     = ak;
            v["secret_access_key"] = sk;
            providers["volcengine"] = v;
        }
    }

    QJsonObject newCfg;
    newCfg["providers"] = providers;

    // 备份
    QFile::copy(path, path + ".bak");
    qInfo() << "[config] Backup saved to" << path + ".bak";

    QString out = QString::fromUtf8(QJsonDocument(newCfg).toJson(QJsonDocument::Indented));
    if (!writeFile(path, out)) {
        qWarning() << "[config] Migration write failed";
        return false;
    }
    qInfo() << "[config] Migration complete";
    return true;
}

void QuotaConfig::createExampleConfig(const QString& path)
{
    QFileInfo fi(path);
    QDir().mkpath(fi.dir().absolutePath());
    writeFile(path, exampleConfig());
}

void QuotaConfig::openConfigFile() const
{
    if (configPath_.isEmpty()) return;
    QFileInfo fi(configPath_);
    if (fi.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(configPath_));
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
    }
}

void QuotaConfig::openConfigDir() const
{
    QFileInfo fi(configPath_);
    QString dir = fi.dir().absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString QuotaConfig::colorFor(int percent)
{
    if (percent <= 0)  return QString::fromUtf8("red");
    if (percent < 10)  return QString::fromUtf8("red");
    if (percent < 50)  return QString::fromUtf8("yellow");
    return QString::fromUtf8("green");
}
