#include "LogHandler.h"

// 初始化 static 变量
QMutex LogHandlerPrivate::logMutex;
QFile* LogHandlerPrivate::logFile = nullptr;
QTextStream* LogHandlerPrivate::logOut = nullptr;

LogHandlerPrivate::LogHandlerPrivate() {
    logDir.setPath("log");
    if (!logDir.exists())
        logDir.mkpath(".");

    //保留10份历史log
    logDir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);      // 过滤掉 ". or .." 文件
    QFileInfoList fileList = logDir.entryInfoList();
    int currentCount = logDir.count();
    foreach (QFileInfo f, fileList ) {
        std::cout << f.baseName().toStdString() <<std::endl;
        if (currentCount <= 999)
            break;
        logDir.remove(f.absoluteFilePath());
        currentCount--;
    }

    // 新建本次启动log文件
    QString logPath = logDir.absoluteFilePath(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss.log"));
    if (logFile == nullptr) {
        logFile = new QFile(logPath);
        logOut  = (logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) ?  new QTextStream(logFile) : nullptr;
        if (logOut != nullptr)
            logOut->setCodec("UTF-8");
    }

    // 定时刷新日志输出缓存到文件，单位ms
    flushLogFileTimer.setInterval(10);
    flushLogFileTimer.start();
    QObject::connect(&flushLogFileTimer, &QTimer::timeout, [] {
        QMutexLocker locker(&LogHandlerPrivate::logMutex);
        if (nullptr != logOut)
            logOut->flush();
    });
}

LogHandlerPrivate::~LogHandlerPrivate() {
    if (nullptr != logFile) {
        logFile->flush();
        logFile->close();
        delete logOut;
        delete logFile;

        logOut  = nullptr;
        logFile = nullptr;
    }
}

void LogHandlerPrivate::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QMutexLocker locker(&LogHandlerPrivate::logMutex);
    QString level;

    switch (type) {
    case QtDebugMsg:
        level = "debug";
        break;
    case QtInfoMsg:
        level = "info ";
        break;
    case QtWarningMsg:
        level = "warn ";
        break;
    case QtCriticalMsg:
        level = "err  ";
        break;
    case QtFatalMsg:
        level = "fatal";
        break;
    default:
        break;
    }

// 输出到标准输出: Windows 下 std::cout 使用 GB2312，而 msg 使用 UTF-8，但是程序的 Local 也还是使用 UTF-8
#if defined(Q_OS_WIN)
    QByteArray localMsg = QTextCodec::codecForName("GB2312")->fromUnicode(msg); //msg.toLocal8Bit();
#else
    QByteArray localMsg = msg.toLocal8Bit();
#endif

    std::cout << std::string(localMsg) << std::endl;

    if (nullptr == LogHandlerPrivate::logOut)
        return;

    // 输出到日志文件, 格式: 时间 [Level] 函数 行数: 消息
    (*LogHandlerPrivate::logOut) << QString("%1 [%2] %3 %4: %5\n")
                                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                                        .arg(level, -5, ' ').arg(context.line).arg(context.function).arg(msg);
}

LogHandler::LogHandler() : d(nullptr) {}

// 给Qt安装消息处理函数
void LogHandler::installMessageHandler() {
    QMutexLocker locker(&LogHandlerPrivate::logMutex); // 类似C++11的lock_guard，析构时自动解锁

    if (nullptr == d) {
        d = new LogHandlerPrivate();
        qInstallMessageHandler(LogHandlerPrivate::messageHandler); // 给 Qt 安装自定义消息处理函数
    }
}

// 取消安装消息处理函数并释放资源
void LogHandler::uninstallMessageHandler() {
    QMutexLocker locker(&LogHandlerPrivate::logMutex);

    qInstallMessageHandler(nullptr);
    delete d;
    d = nullptr;
}
