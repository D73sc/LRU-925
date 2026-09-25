#include "IniHandler.h"
#include <QDebug>
#include <QFileInfo>

IniHandler::IniHandler(QObject *parent)
    : QObject(parent)
{
}

QSettings* IniHandler::createSettings(const QString& filePath)
{
    // 检查文件是否存在，不存在则创建
    QFileInfo fileInfo(filePath);
    if(!fileInfo.exists()) {
        QFile file(filePath);
        if(!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create ini file:" << filePath;
            return nullptr;
        }
        file.close();
    }

    return new QSettings(filePath, QSettings::IniFormat);
}

QString IniHandler::readString(const QString& filePath,
                               const QString& section,
                               const QString& key,
                               const QString& defaultValue)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return defaultValue;

    settings->beginGroup(section);
    QVariant value = settings->value(key, defaultValue);
    settings->endGroup();

    return value.toString();
}

bool IniHandler::writeString(const QString& filePath,
                             const QString& section,
                             const QString& key,
                             const QString& value)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return false;

    settings->beginGroup(section);
    settings->setValue(key, value);
    settings->endGroup();

    // 立即同步写入磁盘
    settings->sync();
    return (settings->status() == QSettings::NoError);
}

int IniHandler::readInt(const QString& filePath,
                        const QString& section,
                        const QString& key,
                        int defaultValue)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return defaultValue;

    settings->beginGroup(section);
    QVariant value = settings->value(key, defaultValue);
    settings->endGroup();

    return value.toInt();
}

bool IniHandler::writeInt(const QString& filePath,
                          const QString& section,
                          const QString& key,
                          int value)
{
    return writeString(filePath, section, key, QString::number(value));
}

bool IniHandler::readBool(const QString& filePath,
                          const QString& section,
                          const QString& key,
                          bool defaultValue)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return defaultValue;

    settings->beginGroup(section);
    QVariant value = settings->value(key, defaultValue);
    settings->endGroup();

    return value.toBool();
}

bool IniHandler::writeBool(const QString& filePath,
                           const QString& section,
                           const QString& key,
                           bool value)
{
    return writeString(filePath, section, key, value ? "true" : "false");
}

bool IniHandler::contains(const QString& filePath,
                          const QString& section,
                          const QString& key)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return false;

    settings->beginGroup(section);
    bool exists = settings->contains(key);
    settings->endGroup();

    return exists;
}

bool IniHandler::removeKey(const QString& filePath,
                           const QString& section,
                           const QString& key)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return false;

    settings->beginGroup(section);
    settings->remove(key);
    settings->endGroup();

    settings->sync();
    return (settings->status() == QSettings::NoError);
}

QStringList IniHandler::getAllKeys(const QString& filePath,
                                   const QString& section)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return QStringList();

    settings->beginGroup(section);
    QStringList keys = settings->childKeys();
    settings->endGroup();

    return keys;
}

QStringList IniHandler::getAllSections(const QString& filePath)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if(!settings) return QStringList();

    return settings->childGroups();
}

//高级功能
// 字符串列表处理
bool IniHandler::writeStringList(const QString& filePath,
                                 const QString& section,
                                 const QString& key,
                                 const QStringList& values,
                                 const QString& separator)
{
    if (values.isEmpty()) return false;
    return writeString(filePath, section, key, values.join(separator));
}

QStringList IniHandler::readStringList(const QString& filePath,
                                       const QString& section,
                                       const QString& key,
                                       const QString& separator)
{
    QString data = readString(filePath, section, key);
    return data.split(separator, Qt::SkipEmptyParts);
}

// 简单加密实现
QByteArray IniHandler::simpleEncrypt(const QByteArray &data, const QByteArray &key)
{
    QByteArray result(data);
    const QByteArray realKey = deriveKey(key);

    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ realKey[i % realKey.size()];
    }
    return result.toBase64();
}

QByteArray IniHandler::simpleDecrypt(const QByteArray &data, const QByteArray &key)
{
    QByteArray result = QByteArray::fromBase64(data);
    const QByteArray realKey = deriveKey(key);

    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ realKey[i % realKey.size()];
    }
    return result;
}

QByteArray IniHandler::deriveKey(const QByteArray &password)
{
    return QCryptographicHash::hash(password, QCryptographicHash::Sha256);
}

// 加密存储
bool IniHandler::writeEncryptedString(const QString& filePath,
                                      const QString& section,
                                      const QString& key,
                                      const QString& value,
                                      const QByteArray& password)
{
    const QByteArray encrypted = simpleEncrypt(value.toUtf8(), password);
    return writeString(filePath, section, key, QString::fromLatin1(encrypted));
}

QString IniHandler::readEncryptedString(const QString& filePath,
                                        const QString& section,
                                        const QString& key,
                                        const QByteArray& password)
{
    const QString encrypted = readString(filePath, section, key);
    if (encrypted.isEmpty()) return "";

    const QByteArray decrypted = simpleDecrypt(encrypted.toLatin1(), password);
    return QString::fromUtf8(decrypted);
}

// 版本管理
bool IniHandler::setConfigVersion(const QString& filePath, const QString& version)
{
    return writeString(filePath, "Metadata", "ConfigVersion", version);
}

QString IniHandler::getConfigVersion(const QString& filePath)
{
    return readString(filePath, "Metadata", "ConfigVersion", "1.0");
}

// 配置迁移
bool IniHandler::migrateConfig(const QString& filePath,
                               const QString& targetVersion,
                               std::function<bool(QSettings&)> migrationFunc)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if (!settings) return false;

    const QString currentVersion = getConfigVersion(filePath);
    if (currentVersion == targetVersion) return true;

    if (migrationFunc && migrationFunc(*settings)) {
        setConfigVersion(filePath, targetVersion);
        settings->sync();
        return true;
    }
    return false;
}

// 自动保存管理
static QHash<QString, bool> autoSaveStatus;

bool IniHandler::autoSaveEnabled(const QString& filePath, bool enable)
{
    autoSaveStatus[filePath] = enable;
    if (!enable) return saveAllPending(filePath);
    return true;
}

bool IniHandler::saveAllPending(const QString& filePath)
{
    QScopedPointer<QSettings> settings(createSettings(filePath));
    if (!settings) return false;
    settings->sync();
    return (settings->status() == QSettings::NoError);
}

/*

高级功能使用示例

// 写入字符串列表
QStringList servers = {"192.168.1.1", "192.168.1.2"};
IniHandler::writeStringList("config.ini", "Network", "Servers", servers);

// 读取字符串列表
QStringList serverList = IniHandler::readStringList("config.ini", "Network", "Servers");



const QByteArray password = "MySecretKey123!";

// 写入加密数据
IniHandler::writeEncryptedString("secure.ini", "Auth", "Token",
                                "SensitiveData123", password);

// 读取加密数据
QString token = IniHandler::readEncryptedString("secure.ini", "Auth",
                                               "Token", password);

// 版本迁移函数示例
bool migrateV1ToV2(QSettings& settings) {
    if (settings.contains("OldSection/OldKey")) {
        QString value = settings.value("OldSection/OldKey").toString();
        settings.remove("OldSection/OldKey");
        settings.setValue("NewSection/NewKey", value + "_migrated");
        return true;
    }
    return false;
}

// 执行迁移
IniHandler::migrateConfig("app_config.ini", "2.0", migrateV1ToV2);

// 启用自动保存
IniHandler::autoSaveEnabled("user_prefs.ini", true);

// 批量写入操作
IniHandler::writeInt("user_prefs.ini", "UI", "Width", 1024);
IniHandler::writeString("user_prefs.ini", "UI", "Theme", "Dark");

// 手动立即保存
IniHandler::saveAllPending("user_prefs.ini");


*/
