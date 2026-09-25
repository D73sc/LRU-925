#ifndef LOGHANDLER_H
#define LOGHANDLER_H

#include <iostream>
#include <QDebug>
#include <QDateTime>
#include <QMutexLocker>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QTextStream>
#include <QTextCodec>

const int g_logLimitSize = 5;

struct LogHandlerPrivate {
    LogHandlerPrivate();
    ~LogHandlerPrivate();

    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    QDir   logDir;              // 日志文件夹
    QTimer flushLogFileTimer;   // 刷新输出到日志文件的定时器

    static QFile *logFile;      // 日志文件
    static QTextStream *logOut; // 输出日志的 QTextStream，使用静态对象就是为了减少函数调用的开销
    static QMutex logMutex;     // 同步使用的 mutex
};

class LogHandler {
public:
    void installMessageHandler();   // 给Qt安装消息处理函数
    void uninstallMessageHandler(); // 取消安装消息处理函数并释放资源

    static LogHandler& Get() {
        static LogHandler m_logHandler;
        return m_logHandler;
    }

private:
    LogHandler();
    LogHandlerPrivate *d;
};

#endif // LOGHANDLER_H
