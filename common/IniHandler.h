#ifndef INIHANDLER_H
#define INIHANDLER_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QCryptographicHash>

class IniHandler : public QObject
{
    Q_OBJECT

public:
    explicit IniHandler(QObject *parent = nullptr);

    // 基本读写操作
    static QString readString(const QString& filePath,
                              const QString& section,
                              const QString& key,
                              const QString& defaultValue = "");

    static bool writeString(const QString& filePath,
                            const QString& section,
                            const QString& key,
                            const QString& value);

    // 数值类型操作
    static int readInt(const QString& filePath,
                       const QString& section,
                       const QString& key,
                       int defaultValue = 0);

    static bool writeInt(const QString& filePath,
                         const QString& section,
                         const QString& key,
                         int value);

    // 布尔值操作
    static bool readBool(const QString& filePath,
                         const QString& section,
                         const QString& key,
                         bool defaultValue = false);

    static bool writeBool(const QString& filePath,
                          const QString& section,
                          const QString& key,
                          bool value);

    // 其他操作
    static bool contains(const QString& filePath,
                         const QString& section,
                         const QString& key);

    static bool removeKey(const QString& filePath,
                          const QString& section,
                          const QString& key);

    static QStringList getAllKeys(const QString& filePath,
                                  const QString& section);

    static QStringList getAllSections(const QString& filePath);
    // 高级功能扩展
    static bool writeStringList(const QString& filePath,
                                const QString& section,
                                const QString& key,
                                const QStringList& values,
                                const QString& separator = "|");

    static QStringList readStringList(const QString& filePath,
                                      const QString& section,
                                      const QString& key,
                                      const QString& separator = "|");

    static bool writeEncryptedString(const QString& filePath,
                                     const QString& section,
                                     const QString& key,
                                     const QString& value,
                                     const QByteArray& password);

    static QString readEncryptedString(const QString& filePath,
                                       const QString& section,
                                       const QString& key,
                                       const QByteArray& password);

    static bool setConfigVersion(const QString& filePath,
                                 const QString& version);

    static QString getConfigVersion(const QString& filePath);

    static bool migrateConfig(const QString& filePath,
                              const QString& targetVersion,
                              std::function<bool(QSettings&)> migrationFunc);

    static bool autoSaveEnabled(const QString& filePath, bool enable);

    static bool saveAllPending(const QString& filePath);
private:
    // 统一创建 QSettings 对象
    static QSettings* createSettings(const QString& filePath);

    // 高级功能扩展
    static QByteArray simpleEncrypt(const QByteArray &data, const QByteArray &key);
    static QByteArray simpleDecrypt(const QByteArray &data, const QByteArray &key);
    static QByteArray deriveKey(const QByteArray &password);
};

#endif // INIHANDLER_H


/*

// 创建配置示例
IniHandler::writeString("config.ini", "Database", "Host", "localhost");
IniHandler::writeInt("config.ini", "Database", "Port", 3306);
IniHandler::writeBool("config.ini", "Settings", "AutoSave", true);

// 读取配置示例
QString host = IniHandler::readString("config.ini", "Database", "Host");
int port = IniHandler::readInt("config.ini", "Database", "Port");
bool autoSave = IniHandler::readBool("config.ini", "Settings", "AutoSave");

// 获取所有配置项
QStringList dbKeys = IniHandler::getAllKeys("config.ini", "Database");
QStringList sections = IniHandler::getAllSections("config.ini");

// 删除配置项
IniHandler::removeKey("config.ini", "Database", "Port");


*/
